/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/Logger.h>
#include <lagrange/python/binding.h>
#include <lagrange/python/tensor_utils.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/warning.h>
#include <lagrange/volume/internal/utils.h>
#include <lagrange/volume/mesh_to_volume.h>
#include <lagrange/volume/volume_to_mesh.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <tbb/enumerable_thread_specific.h>
#include <nanovdb/tools/CreateNanoGrid.h>
#include <nanovdb/tools/NanoToOpenVDB.h>
#include <nanovdb/io/IO.h>
#include <openvdb/io/Stream.h>
#include <lagrange/utils/warnon.h>
// clang-format on

// We use std::istrstream which is deprecated in C++98, and will be removed in C++26.
// Once we move to C++23 we can use std::ispanstream instead.
//
// `#pragma GCC diagnostic ignored "-Wdeprecated-declarations"` doesn't work with GCC's
// backward_warning.h. One can use `#pragma GCC diagnostic ignored "-Wcpp"` to ignore
// #warning pragmas, but this only works with GCC 13+. The hacky workaround is to
// undefine a GCC-specific macro instead.
#if LAGRANGE_TARGET_COMPILER(GCC) && defined(__DEPRECATED)
    #undef __DEPRECATED
    #define LA_RESTORE_DEPRECATED
#endif
// clang-format off
#include <lagrange/utils/warnoff.h>
#include <strstream>
#include <lagrange/utils/warnon.h>
// clang-format on
#ifdef LA_RESTORE_DEPRECATED
    #define __DEPRECATED
#endif

namespace lagrange::python {

namespace nb = nanobind;
using namespace nb::literals;

namespace {

using AllGrids = openvdb::TypeList<openvdb::FloatGrid, openvdb::DoubleGrid>;

template <bool B, typename T>
using MaybeConst = std::conditional_t<B, const T, T>;

template <typename GridPtrType, typename Func>
auto apply_or_fail_(GridPtrType&& grid, Func&& func)
{
    constexpr bool IsConst = std::is_same_v<std::decay_t<GridPtrType>, openvdb::GridBase::ConstPtr>;
    using FloatGridRef = std::add_lvalue_reference_t<MaybeConst<IsConst, openvdb::FloatGrid>>;
    using DoubleGridRef = std::add_lvalue_reference_t<MaybeConst<IsConst, openvdb::DoubleGrid>>;
    static_assert(
        std::is_same_v<
            std::invoke_result_t<Func, FloatGridRef>,
            std::invoke_result_t<Func, DoubleGridRef>>,
        "Func must return the same type for all grid types.");
    using ReturnType = std::invoke_result_t<Func, FloatGridRef>;
    if constexpr (std::is_void_v<ReturnType>) {
        // Void return type
        bool ok = grid->template apply<AllGrids>([&](auto&& real_grid) {
            func(std::forward<decltype(real_grid)>(real_grid));
            return true;
        });
        if (!ok) {
            throw Error("Unsupported grid type.");
        }
        return;
    } else {
        // Non-void return type. To make it work with non-default-constructible types, we
        // cannot use OpenVDB's apply() function.
        if (auto float_grid = openvdb::GridBase::grid<openvdb::FloatGrid>(grid)) {
            return func(*float_grid);
        } else if (auto double_grid = openvdb::GridBase::grid<openvdb::DoubleGrid>(grid)) {
            return func(*double_grid);
        } else {
            throw Error("Unsupported grid type.");
        }
    }
}

template <typename Func>
auto apply_or_fail(const openvdb::GridBase::ConstPtr& grid, Func&& func)
{
    return apply_or_fail_(grid, std::forward<Func>(func));
}

template <typename Func>
auto apply_or_fail(openvdb::GridBase::Ptr& grid, Func&& func)
{
    return apply_or_fail_(grid, std::forward<Func>(func));
}

enum class Compression {
    Uncompressed,
    Zip,
    Blosc,
};

uint32_t to_vdb_compression(Compression compression)
{
    switch (compression) {
    case Compression::Uncompressed: return openvdb::io::COMPRESS_NONE;
    case Compression::Zip: return openvdb::io::COMPRESS_ZIP;
    case Compression::Blosc: return openvdb::io::COMPRESS_BLOSC;
    default: throw Error("Unknown compression type.");
    }
}

nanovdb::io::Codec to_nanovdb_compression(Compression compression)
{
    switch (compression) {
    case Compression::Uncompressed: return nanovdb::io::Codec::NONE;
    case Compression::Zip: return nanovdb::io::Codec::ZIP;
    case Compression::Blosc: return nanovdb::io::Codec::BLOSC;
    default: throw Error("Unknown compression type.");
    }
}

struct GridWrapper
{
    GridWrapper(openvdb::GridBase::Ptr grid)
        : m_grid(std::move(grid))
    {}
    GridWrapper() = default;
    GridWrapper(const GridWrapper&) = default;
    GridWrapper(GridWrapper&&) = default;
    GridWrapper& operator=(const GridWrapper&) = default;
    GridWrapper& operator=(GridWrapper&&) = default;
    openvdb::GridBase::ConstPtr grid() const { return m_grid; }
    openvdb::GridBase::Ptr& grid() { return m_grid; }

private:
    openvdb::GridBase::Ptr m_grid;
};

template <typename GridType>
struct GridSampler
{
    using Accessor = typename GridType::ConstAccessor;
    using Sampler = openvdb::tools::GridSampler<Accessor, openvdb::tools::BoxSampler>;
    GridSampler(const GridType& grid)
        : accessor(grid.getConstAccessor())
        , sampler(accessor, grid.transform())
    {}
    Accessor accessor;
    Sampler sampler;
};

openvdb::GridBase::Ptr grid_from_data(std::variant<fs::path, nb::bytes> input_path_or_buffer)
{
    if (const auto* input_path = std::get_if<fs::path>(&input_path_or_buffer)) {
        // Load grid from file
        logger().info("Loading grid from file: {}", input_path->string());
        if (input_path->extension() == ".vdb") {
            openvdb::io::File file(input_path->string());
            file.open();
            auto grids = file.getGrids();
            file.close();
            la_runtime_assert(
                grids && grids->size() == 1,
                "Input VDB must contain exactly one grid.");
            return (*grids)[0];
        } else if (input_path->extension() == ".nvdb") {
            auto handle = nanovdb::io::readGrid(input_path->string());
            return nanovdb::tools::nanoToOpenVDB(handle);
        } else {
            throw Error("Unsupported input file extension: " + input_path->extension().string());
        }
    } else if (const auto* input_buffer = std::get_if<nb::bytes>(&input_path_or_buffer)) {
        // Load grid from buffer
        logger().info("Loading grid from memory buffer of size {}", input_buffer->size());
        try {
            LA_IGNORE_DEPRECATION_WARNING_BEGIN
            // TODO: Switch to std::ispanstream() when we can use C++23 (probably when I
            // retire...)
            std::istrstream istr(input_buffer->c_str(), input_buffer->size());
            LA_IGNORE_DEPRECATION_WARNING_END
            openvdb::io::Stream strm(istr, false /* delayLoad */);
            auto grids = strm.getGrids();
            la_runtime_assert(
                grids && grids->size() == 1,
                "Input VDB must contain exactly one grid.");
            return (*grids)[0];
        } catch (const openvdb::IoError&) {
            // Not a VDB grid, try NanoVDB
            LA_IGNORE_DEPRECATION_WARNING_BEGIN
            // TODO: Switch to std::ispanstream() when we can use C++23 (probably when I
            // retire...)
            std::istrstream istr(input_buffer->c_str(), input_buffer->size());
            LA_IGNORE_DEPRECATION_WARNING_END
            try {
                auto handles = nanovdb::io::readGrids(istr);
                la_runtime_assert(
                    handles.size() == 1,
                    "Input NanoVDB must contain exactly one grid.");
                return nanovdb::tools::nanoToOpenVDB(handles[0]);
            } catch (const std::runtime_error&) {
                throw Error("Invalid input grid: not a valid VDB or NanoVDB grid.");
            }
        }
    } else {
        throw Error("Invalid input grid: not a path or buffer.");
    }
}

template <typename GridScalar>
void grid_to_file(
    typename lagrange::volume::Grid<GridScalar>::ConstPtr grid,
    const fs::path& output_path,
    Compression compression)
{
    if (output_path.extension() == ".vdb") {
        openvdb::io::File file(output_path.string());
        file.setCompression(to_vdb_compression(compression));
        file.write({grid});
        file.close();
    } else if (output_path.extension() == ".nvdb") {
        auto handle = nanovdb::tools::createNanoGrid(*grid);
        nanovdb::io::writeGrid(output_path.string(), handle, to_nanovdb_compression(compression));
    } else {
        throw Error("Unsupported output file extension: " + output_path.string());
    }
}

void save(const GridWrapper& self, const fs::path& output_path, Compression compression)
{
    apply_or_fail(self.grid(), [&](auto&& grid) {
        using RealGrid = std::decay_t<decltype(grid)>;
        using GridScalar = typename RealGrid::ValueType;
        grid_to_file<GridScalar>(
            openvdb::gridConstPtrCast<RealGrid>(self.grid()),
            output_path,
            compression);
    });
}

template <typename GridScalar>
std::string grid_to_buffer(
    typename lagrange::volume::Grid<GridScalar>::ConstPtr grid,
    const std::string& ext,
    Compression compression)
{
    std::ostringstream output_stream(std::ios_base::binary);
    if (ext == "vdb") {
        openvdb::io::Stream oss(output_stream);
        oss.setCompression(to_vdb_compression(compression));
        oss.write({grid});
    } else if (ext == "nvdb") {
        auto handle = nanovdb::tools::createNanoGrid(*grid);
        nanovdb::io::writeGrid(output_stream, handle, to_nanovdb_compression(compression));
    } else {
        throw Error("Unsupported grid extension: " + ext);
    }

    return output_stream.str();
}

nb::bytes save_to_buffer(const GridWrapper& self, const std::string& ext, Compression compression)
{
    std::string buffer = apply_or_fail(self.grid(), [&](auto&& grid) {
        using RealGrid = std::decay_t<decltype(grid)>;
        using GridScalar = typename RealGrid::ValueType;
        return grid_to_buffer<GridScalar>(
            openvdb::gridConstPtrCast<RealGrid>(self.grid()),
            ext,
            compression);
    });
    return nb::bytes(buffer.data(), buffer.size());
}

// I had to pull this out as a separate function (and not a lambda), due to an ICE on VS 2022
template <typename IndexArray, typename VecType>
std::variant<Eigen::VectorXf, Eigen::VectorXd>
sample_trilinear_index_space(const GridWrapper& self, IndexArray& idx, VecType)
{
    auto v = idx.view();
    std::variant<Eigen::VectorXf, Eigen::VectorXd> result_var;
    apply_or_fail(self.grid(), [&](auto&& grid) {
        using GridType = std::decay_t<decltype(grid)>;
        using GridScalar = typename GridType::ValueType;
        Eigen::VectorX<GridScalar> result(v.shape(0));
        tbb::enumerable_thread_specific<GridSampler<GridType>> data(
            [&] { return GridSampler<GridType>(grid); });
        tbb::parallel_for(tbb::blocked_range<size_t>(0, v.shape(0)), [&](const auto& r) {
            auto& sampler = data.local().sampler;
            for (size_t i = r.begin(); i != r.end(); ++i) {
                result(i) = sampler.isSample(VecType(v(i, 0), v(i, 1), v(i, 2)));
            }
        });
        result_var = std::move(result);
    });
    return result_var;
};

} // namespace

void populate_volume_module(nb::module_& m)
{
    using Scalar = double;
    using Index = uint32_t;

    using ConstArray3i =
        nb::ndarray<const int32_t, nb::shape<-1, 3>, nb::c_contig, nb::device::cpu>;
    using ConstArray3d = nb::ndarray<const double, nb::shape<-1, 3>, nb::c_contig, nb::device::cpu>;
    using ConstArray3f = nb::ndarray<const float, nb::shape<-1, 3>, nb::c_contig, nb::device::cpu>;

    using Sign = lagrange::volume::MeshToVolumeOptions::Sign;
    nb::enum_<Sign>(m, "Sign", "Signing method used to determine inside/outside voxels")
        .value("FloodFill", Sign::FloodFill, "Default voxel flood-fill method used by OpenVDB.")
        .value(
            "WindingNumber",
            Sign::WindingNumber,
            "Fast winding number method based on [Barill et al. 2018].")
        .value("Unsigned", Sign::Unsigned, "Do not compute sign, output unsigned distance field.");

    nb::enum_<Compression>(m, "Compression", "Compression method for VDB and NanoVDB grid IO")
        .value("Uncompressed", Compression::Uncompressed, "No compression.")
        .value("Zip", Compression::Zip, "Zip compression.")
        .value("Blosc", Compression::Blosc, "Blosc compression.");

    auto float_type = [] {
        auto np = nb::module_::import_("numpy");
        return np.attr("float32");
    }();

    nb::class_<GridWrapper> g(m, "Grid");

    g.def_static(
        "load",
        [](std::variant<fs::path, nb::bytes> input_path_or_buffer) {
            GridWrapper wrapper(grid_from_data(input_path_or_buffer));
            return wrapper;
        },
        "input_path_or_buffer"_a,
        R"(Load a grid from a file or memory buffer.

:param input_path_or_buffer: Input file path (str or pathlib.Path) or memory buffer (bytes).

:returns: Loaded grid.)");

    g.def(
        "save",
        &save,
        "output_path"_a,
        nb::arg("compression") = Compression::Blosc,
        R"(Save the grid to a file.

:param output_path: Output file path (str or pathlib.Path).
:param compression: Compression method to use. Default is Blosc.)");

    g.def(
        "to_buffer",
        &save_to_buffer,
        "grid_type"_a,
        nb::arg("compression") = Compression::Blosc,
        R"(Save the grid to a memory buffer.

:param grid_type: Output grid type, either 'vdb' or 'nvdb'.
:param compression: Compression method to use. Default is Blosc.

:returns: Memory buffer (bytes).)",
        nb::sig(
            "def to_buffer(ext: typing.Literal['vdb', 'nvdb'], compression: Compression = "
            "Compression.Blosc) -> bytes"));

    g.def_prop_ro(
        "voxel_size",
        [](const GridWrapper& self) {
            auto vs = self.grid()->voxelSize();
            Eigen::Vector3d result;
            result << vs.x(), vs.y(), vs.z();
            return result;
        },
        "Return the grid voxel size.");

    g.def_prop_ro(
        "num_active_voxels",
        [](const GridWrapper& self) { return self.grid()->activeVoxelCount(); },
        "Return the number of active voxels in the grid.");

    g.def_prop_ro(
        "bbox_index",
        [](const GridWrapper& self) {
            const auto bbox = self.grid()->evalActiveVoxelBoundingBox();
            Eigen::Matrix<int32_t, 2, 3> result;
            result << bbox.min().x(), bbox.min().y(), bbox.min().z(), bbox.max().x(),
                bbox.max().y(), bbox.max().z();
            return result;
        },
        "Return the axis-aligned bounding box of all active voxels in index space. If the grid is "
        "empty a default bbox is returned.");

    g.def_prop_ro(
        "bbox_world",
        [](const GridWrapper& self) {
            const auto bbox_index = self.grid()->evalActiveVoxelBoundingBox();
            auto bbox = self.grid()->transform().indexToWorld(bbox_index);
            Eigen::Matrix<Scalar, 2, 3> result;
            result << bbox.min().x(), bbox.min().y(), bbox.min().z(), bbox.max().x(),
                bbox.max().y(), bbox.max().z();
            return result;
        },
        "Return the axis-aligned bounding box of all active voxels in world space. If the grid is "
        "empty a default bbox is returned.");

    g.def(
        "index_to_world",
        [](const GridWrapper& self,
           std::variant<ConstArray3i, ConstArray3f, ConstArray3d> indices) {
            auto run = [&](auto&& idx, auto&& vec, auto&& result) {
                using VecType = std::decay_t<decltype(vec)>;
                auto v = idx.view();
                result.resize(v.shape(0), 3);
                const auto transform = self.grid()->transform();
                tbb::parallel_for(size_t(0), v.shape(0), [&](size_t i) {
                    auto pos = transform.indexToWorld(VecType(v(i, 0), v(i, 1), v(i, 2)));
                    result.row(i) << pos.x(), pos.y(), pos.z();
                });
                return result;
            };
            return std::visit(
                [&](auto&& idx) -> std::variant<Eigen::MatrixX3f, Eigen::MatrixX3d> {
                    using IdxType = std::decay_t<decltype(idx)>;
                    if constexpr (std::is_same_v<IdxType, ConstArray3i>) {
                        return run(idx, openvdb::Coord(), Eigen::MatrixX3d());
                    } else if constexpr (std::is_same_v<IdxType, ConstArray3f>) {
                        return run(idx, openvdb::Vec3d(), Eigen::MatrixX3f());
                    } else if constexpr (std::is_same_v<IdxType, ConstArray3d>) {
                        return run(idx, openvdb::Vec3d(), Eigen::MatrixX3d());
                    } else {
                        throw nb::type_error("Invalid index array type.");
                    }
                },
                indices);
        },
        "indices"_a,
        R"(Convert a set of voxel indices to world coordinates.

:param indices: Input voxel indices as an (N, 3) array of integers or double.

:returns: World coordinates as an (N, 3) array of double.)");

    g.def(
        "world_to_index",
        [](const GridWrapper& self, std::variant<ConstArray3f, ConstArray3d> points) {
            auto run = [&](auto&& pts, auto&& result) {
                auto v = pts.view();
                result.resize(v.shape(0), 3);
                const auto transform = self.grid()->transform();
                tbb::parallel_for(size_t(0), v.shape(0), [&](size_t i) {
                    auto coord = transform.worldToIndex(openvdb::Vec3d(v(i, 0), v(i, 1), v(i, 2)));
                    result.row(i) << coord.x(), coord.y(), coord.z();
                });
                return result;
            };
            return std::visit(
                [&](auto&& pts) -> std::variant<Eigen::MatrixX3f, Eigen::MatrixX3d> {
                    using PtsType = std::decay_t<decltype(pts)>;
                    if constexpr (std::is_same_v<PtsType, ConstArray3f>) {
                        return run(pts, Eigen::MatrixX3f());
                    } else if constexpr (std::is_same_v<PtsType, ConstArray3d>) {
                        return run(pts, Eigen::MatrixX3d());
                    } else {
                        throw nb::type_error("Invalid points array type.");
                    }
                },
                points);
        },
        "points"_a,
        R"(Convert a set of world coordinates to voxel indices.

:param points: Input world coordinates as an (N, 3) array of double.

:returns: Voxel indices as an (N, 3) array of double.)");

    g.def(
        "resample",
        [](const GridWrapper& self, double voxel_size) {
            GridWrapper result;
            apply_or_fail(self.grid(), [&](auto&& grid) {
                result.grid() = lagrange::volume::internal::resample_grid(grid, voxel_size);
            });
            return result;
        },
        "voxel_size"_a,
        R"(Resample the grid to a new voxel size.

:param voxel_size: New voxel size. Negative means relative to the input grid voxel size.

:returns: Resampled grid.)");

    g.def(
        "densify",
        [](const GridWrapper& self) {
            GridWrapper result;
            apply_or_fail(self.grid(), [&](auto&& grid) {
                result.grid() = lagrange::volume::internal::densify_grid(grid);
            });
            return result;
        },
        R"(Densify the grid by filling in inactive voxels within the active voxel bounding box.

:returns: Densified grid.)");

    g.def(
        "redistance",
        [](const GridWrapper& self) {
            GridWrapper result;
            apply_or_fail(self.grid(), [&](auto&& grid) {
                result.grid() = lagrange::volume::internal::redistance_grid(grid);
            });
            return result;
        },
        R"(Recompute the signed distance values of the grid using a fast sweeping method.

:returns: Redistanced grid.)");

    g.def(
        "offset_in_place",
        [](GridWrapper& self, double offset_radius, bool relative) {
            apply_or_fail(self.grid(), [&](auto&& grid) {
                lagrange::volume::internal::offset_grid_in_place(grid, offset_radius, relative);
            });
        },
        "offset_radius"_a,
        "relative"_a = false,
        R"(Apply an offset to the signed distance field in-place.

:param offset_radius: Offset radius. A negative value dilates the surface, a positive value erodes it.
:param relative: Whether the offset radius is relative to the grid voxel size.)");

    g.def(
        "sample_trilinear_index_space",
        [](const GridWrapper& self, std::variant<ConstArray3i, ConstArray3f, ConstArray3d> indices)
            -> std::variant<Eigen::VectorXf, Eigen::VectorXd> {
            return std::visit(
                [&](auto&& idx) {
                    using IdxType = std::decay_t<decltype(idx)>;
                    if constexpr (std::is_same_v<IdxType, ConstArray3i>) {
                        return sample_trilinear_index_space(self, idx, openvdb::Coord());
                    } else if constexpr (std::is_same_v<IdxType, ConstArray3f>) {
                        return sample_trilinear_index_space(self, idx, openvdb::Vec3d());
                    } else if constexpr (std::is_same_v<IdxType, ConstArray3d>) {
                        return sample_trilinear_index_space(self, idx, openvdb::Vec3d());
                    } else {
                        throw nb::type_error("Invalid index array type.");
                    }
                },
                indices);
        },
        "points"_a,
        R"(Sample the grid at the given world coordinates using trilinear interpolation.

:param points: Input index coordinates as an (N, 3) array of integers or double.

:returns: Sampled values as an (N,) array of double.)");

    g.def(
        "sample_trilinear_world_space",
        [](const GridWrapper& self, std::variant<ConstArray3f, ConstArray3d> points) {
            auto run = [&](auto&& pts) -> std::variant<Eigen::VectorXf, Eigen::VectorXd> {
                auto v = pts.view();
                std::variant<Eigen::VectorXf, Eigen::VectorXd> result_var;
                apply_or_fail(self.grid(), [&](auto&& grid) {
                    using GridType = std::decay_t<decltype(grid)>;
                    using GridScalar = typename GridType::ValueType;
                    Eigen::VectorX<GridScalar> result(v.shape(0));
                    tbb::enumerable_thread_specific<GridSampler<GridType>> data(
                        [&] { return GridSampler<GridType>(grid); });
                    tbb::parallel_for(
                        tbb::blocked_range<size_t>(0, v.shape(0)),
                        [&](const auto& r) {
                            auto& sampler = data.local().sampler;
                            for (size_t i = r.begin(); i != r.end(); ++i) {
                                auto pos = openvdb::Vec3d(v(i, 0), v(i, 1), v(i, 2));
                                result(i) = sampler.wsSample(pos);
                            }
                        });
                    result_var = std::move(result);
                });
                return result_var;
            };
            return std::visit(run, points);
        },
        "points"_a,
        R"(Sample the grid at the given world coordinates using trilinear interpolation.

:param points: Input world coordinates as an (N, 3) array of double.

:returns: Sampled values as an (N,) array of double.)");

    using MeshToVolumeOptions = lagrange::volume::MeshToVolumeOptions;
    g.def_static(
        "from_mesh",
        [](const SurfaceMesh<Scalar, Index>& mesh,
           double voxel_size,
           Sign signing_method,
           nb::type_object dtype) {
            lagrange::volume::MeshToVolumeOptions options;
            options.voxel_size = voxel_size;
            options.signing_method = signing_method;

            auto run = [&](auto&& grid_scalar) -> GridWrapper {
                using GridScalar = std::decay_t<decltype(grid_scalar)>;
                auto grid = lagrange::volume::mesh_to_volume<GridScalar>(mesh, options);
                return GridWrapper{grid};
            };

            auto np = nb::module_::import_("numpy");
            if (dtype.is(np.attr("float32"))) {
                return run(float(0));
            } else if (dtype.is(np.attr("float64")) || dtype.is(&PyLong_Type)) {
                return run(double(0));
            } else {
                throw nb::type_error("Unsupported grid `dtype`!");
            }
        },
        "mesh"_a,
        "voxel_size"_a = MeshToVolumeOptions().voxel_size,
        "signing_method"_a = MeshToVolumeOptions().signing_method,
        "dtype"_a = float_type,
        R"(Convert a triangle mesh to a sparse voxel grid, writing the result to a file.

:param mesh: Input mesh. Must be a triangle mesh, a quad-mesh, or a quad-dominant mesh.
:param voxel_size: Voxel size. Negative means relative to bbox diagonal (`vs -> -vs * bbox_diag`).
:param signing_method: Method used to compute the sign of the distance field.
:param dtype: Scalar type of the output grid (float32 or float64).

:returns: Generated sparse voxel grid.)");

    g.def(
        "to_mesh",
        [](const GridWrapper& self,
           double isovalue,
           double adaptivity,
           bool relax_disoriented_triangles,
           std::optional<std::string_view> normal_attribute_name) {
            lagrange::volume::VolumeToMeshOptions options;
            options.isovalue = isovalue;
            options.adaptivity = adaptivity;
            options.relax_disoriented_triangles = relax_disoriented_triangles;
            options.normal_attribute_name =
                normal_attribute_name.value_or(options.normal_attribute_name);

            SurfaceMesh<Scalar, Index> mesh;
            bool ok = self.grid()->apply<AllGrids>([&](auto&& grid) {
                mesh = lagrange::volume::volume_to_mesh<SurfaceMesh<Scalar, Index>>(grid, options);
            });
            if (!ok) {
                throw Error("Failed to mesh isosurface: unsupported grid type.");
            }
            return mesh;
        },
        "isovalue"_a = lagrange::volume::VolumeToMeshOptions().isovalue,
        "adaptivity"_a = lagrange::volume::VolumeToMeshOptions().adaptivity,
        "relax_disoriented_triangles"_a =
            lagrange::volume::VolumeToMeshOptions().relax_disoriented_triangles,
        "normal_attribute_name"_a = std::nullopt,
        R"(Mesh the isosurface of a sparse voxel grid.

:param grid: Input grid to mesh.
:param isovalue: Value of the isosurface.
:param adaptivity: Surface adaptivity threshold [0 to 1]. 0 keeps the original quad mesh, while 1 simplifies the most.
:param relax_disoriented_triangles: Toggle relaxing disoriented triangles during adaptive meshing.
:param normal_attribute_name: If provided, computes vertex normals from the volume and store them in the appropriately named attribute.

:returns: Meshed isosurface.)");
}

} // namespace lagrange::python

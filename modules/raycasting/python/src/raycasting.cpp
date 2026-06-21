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

#include <lagrange/python/raycasting.h>

#include <lagrange/python/binding.h>
#include <lagrange/python/tensor_utils.h>
#include <lagrange/raycasting/Options.h>
#include <lagrange/raycasting/RayCaster.h>
#include <lagrange/raycasting/compute_local_feature_size.h>
#include <lagrange/raycasting/project.h>
#include <lagrange/raycasting/project_closest_point.h>
#include <lagrange/raycasting/project_closest_vertex.h>
#include <lagrange/raycasting/project_directional.h>
#include <lagrange/raycasting/remove_occluded_facets.h>
#include <lagrange/raycasting/remove_occluded_instances.h>
#include <lagrange/utils/BitField.h>
#include <lagrange/utils/ProgressCallback.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>

#include <tbb/parallel_for.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace nb = nanobind;
using namespace nb::literals;

namespace lagrange::python {

namespace {

using Scalar = double;
using Index = uint32_t;
using MeshType = SurfaceMesh<Scalar, Index>;
// Python None | float scalar | float numpy array
using FloatArray = nb::ndarray<float, nb::numpy, nb::c_contig, nb::device::cpu>;
using FloatParam = std::variant<std::monostate, float, FloatArray>;
// Python None | int (AttributeId) | 3D vector
using DirectionParam = std::variant<std::monostate, AttributeId, Eigen::Vector3f>;

using NDArray3D = nb::ndarray<Scalar, nb::numpy, nb::shape<-1, 3>, nb::c_contig, nb::device::cpu>;

/// Parse a 1D (3,) or 2D (N, 3) float array and return the number of points/rays.
size_t get_num_points(const FloatArray& arr, const char* name)
{
    if (arr.ndim() == 1) {
        if (arr.shape(0) != 3) {
            throw std::invalid_argument(
                std::string(name) + " must have 3 components, got " + std::to_string(arr.shape(0)));
        }
        return 1;
    } else if (arr.ndim() == 2) {
        if (arr.shape(1) != 3) {
            throw std::invalid_argument(
                std::string(name) + " must have shape (N, 3), got (*, " +
                std::to_string(arr.shape(1)) + ")");
        }
        return arr.shape(0);
    } else {
        throw std::invalid_argument(std::string(name) + " must be 1D (3,) or 2D (N, 3)");
    }
}

/// View one row of a 1D (3,) or 2D (N, 3) c-contiguous float array as an Eigen
/// vector. Returns an Eigen::Map aliasing the array's memory; no copy is made,
/// so `arr` must outlive the returned map.
Eigen::Map<const Eigen::Vector3f> get_row(const FloatArray& arr, size_t row)
{
    const float* base = static_cast<const float*>(arr.data()) + (arr.ndim() == 1 ? 0 : row * 3);
    return Eigen::Map<const Eigen::Vector3f>(base);
}

/// Non-owning per-ray view over a FloatParam. A None/scalar parameter is
/// broadcast to all rays via a zero inner stride (every element aliases a single
/// backing float); an array parameter is mapped in place. Reading is `view[i]`;
/// no allocation or copy is performed.
using FloatParamView =
    Eigen::Map<const Eigen::VectorXf, Eigen::Unaligned, Eigen::InnerStride<Eigen::Dynamic>>;

/// Build a FloatParamView (None | scalar | array) over `n` rays. None/scalar
/// values are broadcast through a zero-stride map backed by `scratch`, which
/// must outlive the returned view; an array must have shape (n,) and is mapped
/// in place. `name` identifies the parameter (e.g. "tmin"/"tmax") in error
/// messages.
FloatParamView view_float_param(
    const FloatParam& param,
    size_t n,
    float default_val,
    const char* name,
    float& scratch)
{
    using InnerStride = Eigen::InnerStride<Eigen::Dynamic>;
    const auto len = static_cast<Eigen::Index>(n);
    return std::visit(
        [&](const auto& v) -> FloatParamView {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::monostate> || std::is_same_v<T, float>) {
                if constexpr (std::is_same_v<T, float>) {
                    scratch = v;
                } else {
                    scratch = default_val;
                }
                // Zero inner stride => every element reads the single `scratch`.
                return FloatParamView(&scratch, len, InnerStride(0));
            } else {
                if (v.ndim() != 1 || v.shape(0) != n) {
                    throw std::invalid_argument(
                        std::string(name) + " array must have shape (" + std::to_string(n) +
                        ",) to match the number of rays");
                }
                return FloatParamView(static_cast<const float*>(v.data()), len, InnerStride(1));
            }
        },
        param);
}

// =========================================================================
// Struct-of-arrays results for batch queries.
//
// Building one Python object per ray (and appending to a list) dominates the
// cost of a large batch. Instead we accumulate each field into a contiguous
// buffer and expose them as numpy arrays, which is both faster to fill (the
// loop stays in C++ with the GIL released) and faster to consume (vectorized
// on the Python side). Misses are reported through the `hit` boolean mask; the
// other fields of a missed entry are left at their defaults.
// =========================================================================

/// Struct-of-arrays result of a batch ray cast (one entry per input ray).
struct RayHits
{
    size_t n = 0;
    std::vector<uint8_t> hit; // (n,) 1 = hit, 0 = miss
    std::vector<uint32_t> mesh_index; // (n,)
    std::vector<uint32_t> instance_index; // (n,)
    std::vector<uint32_t> facet_index; // (n,)
    std::vector<float> barycentric_coord; // (n, 2)
    std::vector<float> position; // (n, 3)
    std::vector<float> ray_depth; // (n,)
    std::vector<float> normal; // (n, 3)

    void resize(size_t count)
    {
        n = count;
        hit.assign(n, 0);
        mesh_index.assign(n, lagrange::invalid<uint32_t>());
        instance_index.assign(n, lagrange::invalid<uint32_t>());
        facet_index.assign(n, lagrange::invalid<uint32_t>());
        barycentric_coord.assign(n * 2, 0.0f);
        position.assign(n * 3, 0.0f);
        ray_depth.assign(n, 0.0f);
        normal.assign(n * 3, 0.0f);
    }

    void set(size_t i, const raycasting::RayHit& h)
    {
        hit[i] = 1;
        mesh_index[i] = h.mesh_index;
        instance_index[i] = h.instance_index;
        facet_index[i] = h.facet_index;
        barycentric_coord[2 * i + 0] = h.barycentric_coord[0];
        barycentric_coord[2 * i + 1] = h.barycentric_coord[1];
        position[3 * i + 0] = h.position[0];
        position[3 * i + 1] = h.position[1];
        position[3 * i + 2] = h.position[2];
        ray_depth[i] = h.ray_depth;
        normal[3 * i + 0] = h.normal[0];
        normal[3 * i + 1] = h.normal[1];
        normal[3 * i + 2] = h.normal[2];
    }
};

/// Struct-of-arrays result of a batch closest-point query (one entry per query).
struct ClosestPointHits
{
    size_t n = 0;
    std::vector<uint8_t> hit; // (n,) 1 = hit, 0 = miss
    std::vector<uint32_t> mesh_index; // (n,)
    std::vector<uint32_t> instance_index; // (n,)
    std::vector<uint32_t> facet_index; // (n,)
    std::vector<float> barycentric_coord; // (n, 2)
    std::vector<float> position; // (n, 3)
    std::vector<float> distance; // (n,)

    void resize(size_t count)
    {
        n = count;
        hit.assign(n, 0);
        mesh_index.assign(n, lagrange::invalid<uint32_t>());
        instance_index.assign(n, lagrange::invalid<uint32_t>());
        facet_index.assign(n, lagrange::invalid<uint32_t>());
        barycentric_coord.assign(n * 2, 0.0f);
        position.assign(n * 3, 0.0f);
        distance.assign(n, std::numeric_limits<float>::infinity());
    }

    void set(size_t i, const raycasting::ClosestPointHit& h)
    {
        hit[i] = 1;
        mesh_index[i] = h.mesh_index;
        instance_index[i] = h.instance_index;
        facet_index[i] = h.facet_index;
        barycentric_coord[2 * i + 0] = h.barycentric_coord[0];
        barycentric_coord[2 * i + 1] = h.barycentric_coord[1];
        position[3 * i + 0] = h.position[0];
        position[3 * i + 1] = h.position[1];
        position[3 * i + 2] = h.position[2];
        distance[i] = h.distance;
    }
};

/// numpy array of bool returned by batch occluded() queries.
using BoolArray = nb::ndarray<nb::numpy, bool, nb::ndim<1>>;

/// Wrap a uint8_t buffer as a non-copying numpy bool array of length `n`. The
/// returned array keeps `owner` (a bound result struct) alive.
nb::ndarray<nb::numpy, bool, nb::ndim<1>>
view_hit(std::vector<uint8_t>& v, size_t n, nb::handle owner)
{
    return nb::ndarray<nb::numpy, bool, nb::ndim<1>>(reinterpret_cast<bool*>(v.data()), {n}, owner);
}

/// Wrap a per-element buffer as a non-copying 1D numpy array of length `n`.
template <typename T>
nb::ndarray<nb::numpy, T, nb::ndim<1>> view_1d(std::vector<T>& v, size_t n, nb::handle owner)
{
    return nb::ndarray<nb::numpy, T, nb::ndim<1>>(v.data(), {n}, owner);
}

/// Wrap a flat buffer of `n * Cols` floats as a non-copying (n, Cols) numpy array.
template <int Cols>
nb::ndarray<nb::numpy, float, nb::shape<-1, Cols>>
view_2d(std::vector<float>& v, size_t n, nb::handle owner)
{
    return nb::ndarray<nb::numpy, float, nb::shape<-1, Cols>>(
        v.data(),
        {n, static_cast<size_t>(Cols)},
        owner);
}

std::tuple<span<Scalar>, Shape, Stride> tensor_to_span(NDArray3D tensor)
{
    Shape shape;
    Stride stride;
    size_t size = 1;
    for (size_t i = 0; i < tensor.ndim(); i++) {
        shape.push_back(tensor.shape(i));
        stride.push_back(tensor.stride(i));
        size *= tensor.shape(i);
    }
    span<Scalar> data(static_cast<Scalar*>(tensor.data()), size);
    return {data, shape, stride};
}

MeshType make_point_mesh(NDArray3D tensor)
{
    MeshType mesh;
    auto [values, shape, stride] = tensor_to_span(tensor);
    la_runtime_assert(is_dense(shape, stride));
    la_runtime_assert(check_shape(shape, invalid<size_t>(), mesh.get_dimension()));
    Index num_vertices = static_cast<Index>(shape[0]);

    auto owner = std::make_shared<nb::object>(nb::find(tensor));
    auto id = mesh.wrap_as_vertices(values, num_vertices);
    auto& attr = mesh.template ref_attribute<Scalar>(id);
    attr.set_growth_policy(AttributeGrowthPolicy::ErrorIfExternal);
    attr.set_copy_policy(AttributeCopyPolicy::ErrorIfExternal);
    return mesh;
};

// TODO: Handle Vector3d and NDArray3f as well?
std::variant<std::monostate, Eigen::Vector3f, AttributeId> resolve_direction(
    std::variant<Eigen::Vector3f, NDArray3D> direction,
    SurfaceMesh<Scalar, Index>& mesh)
{
    std::variant<std::monostate, Eigen::Vector3f, AttributeId> result;
    std::visit(
        [&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, Eigen::Vector3f>) {
                result = arg;
            } else {
                // NDArray3D: wrap as a const per-vertex normal attribute.
                auto [values, shape, stride] = tensor_to_span(arg);
                la_runtime_assert(is_dense(shape, stride));
                la_runtime_assert(check_shape(shape, invalid<size_t>(), mesh.get_dimension()));
                auto id = mesh.template wrap_as_const_attribute<Scalar>(
                    "@direction",
                    AttributeElement::Vertex,
                    AttributeUsage::Normal,
                    3,
                    values);
                result = id;
            }
        },
        direction);
    return result;
}

} // namespace

void populate_raycasting_module(nb::module_& m)
{
    // =========================================================================
    // Enums
    // =========================================================================

    nb::enum_<raycasting::SceneFlags>(
        m,
        "SceneFlags",
        nb::is_arithmetic(),
        "Flags for configuring the Embree scene.")
        .value("Empty", raycasting::SceneFlags::None, "No special behavior.")
        .value("Dynamic", raycasting::SceneFlags::Dynamic, "Scene will be updated frequently.")
        .value("Compact", raycasting::SceneFlags::Compact, "Compact BVH layout.")
        .value("Robust", raycasting::SceneFlags::Robust, "Robust BVH traversal.")
        .value("Filter", raycasting::SceneFlags::Filter, "Enable user-defined filters.");

    nb::enum_<raycasting::BuildQuality>(m, "BuildQuality", "BVH construction quality level.")
        .value("Low", raycasting::BuildQuality::Low, "Fastest build time, lowest BVH quality.")
        .value("Medium", raycasting::BuildQuality::Medium, "Moderate build time and BVH quality.")
        .value("High", raycasting::BuildQuality::High, "Slowest build time, highest BVH quality.");

    nb::enum_<raycasting::CastMode>(m, "CastMode", "Ray-casting direction mode.")
        .value("OneWay", raycasting::CastMode::OneWay, "Cast forward only.")
        .value("BothWays", raycasting::CastMode::BothWays, "Cast forward and backward.");

    nb::enum_<raycasting::FallbackMode>(
        m,
        "FallbackMode",
        "Fallback mode for vertices without a ray hit.")
        .value("Constant", raycasting::FallbackMode::Constant, "Fill with a constant value.")
        .value(
            "ClosestVertex",
            raycasting::FallbackMode::ClosestVertex,
            "Copy from the closest vertex.")
        .value(
            "ClosestPoint",
            raycasting::FallbackMode::ClosestPoint,
            "Interpolate from the closest surface point.");

    nb::enum_<raycasting::ProjectMode>(m, "ProjectMode", "Main projection mode.")
        .value(
            "ClosestVertex",
            raycasting::ProjectMode::ClosestVertex,
            "Copy from the closest vertex.")
        .value(
            "ClosestPoint",
            raycasting::ProjectMode::ClosestPoint,
            "Interpolate from the closest surface point.")
        .value(
            "RayCasting",
            raycasting::ProjectMode::RayCasting,
            "Project along a prescribed direction.");

    // =========================================================================
    // RayHit result struct
    // =========================================================================

    nb::class_<raycasting::RayHit>(m, "RayHit", "Result of a ray intersection query.")
        .def_ro("mesh_index", &raycasting::RayHit::mesh_index, "Index of the hit mesh.")
        .def_ro(
            "instance_index",
            &raycasting::RayHit::instance_index,
            "Index of the hit instance (relative to the source mesh).")
        .def_ro("facet_index", &raycasting::RayHit::facet_index, "Index of the hit facet.")
        .def_ro(
            "barycentric_coord",
            &raycasting::RayHit::barycentric_coord,
            R"(Barycentric coordinates ``(u, v)`` of the hit point within the triangle.
The surface point is: ``p = (1 - u - v) * p1 + u * p2 + v * p3``.)")
        .def_ro("position", &raycasting::RayHit::position, "World-space position of the hit point.")
        .def_ro(
            "ray_depth",
            &raycasting::RayHit::ray_depth,
            "Parametric distance along the ray (t value).")
        .def_ro(
            "normal",
            &raycasting::RayHit::normal,
            "Unnormalized geometric normal at the hit point.");

    // =========================================================================
    // ClosestPointHit result struct
    // =========================================================================

    nb::class_<raycasting::ClosestPointHit>(
        m,
        "ClosestPointHit",
        "Result of a closest point query.")
        .def_ro("mesh_index", &raycasting::ClosestPointHit::mesh_index, "Index of the hit mesh.")
        .def_ro(
            "instance_index",
            &raycasting::ClosestPointHit::instance_index,
            "Index of the hit instance (relative to the source mesh).")
        .def_ro("facet_index", &raycasting::ClosestPointHit::facet_index, "Index of the hit facet.")
        .def_ro(
            "barycentric_coord",
            &raycasting::ClosestPointHit::barycentric_coord,
            R"(Barycentric coordinates ``(u, v)`` of the hit point within the triangle.
The surface point is: ``p = (1 - u - v) * p1 + u * p2 + v * p3``.)")
        .def_ro(
            "position",
            &raycasting::ClosestPointHit::position,
            "World-space position of the closest point on the surface.")
        .def_ro(
            "distance",
            &raycasting::ClosestPointHit::distance,
            "Distance from the query point to the closest point on the surface.");

    // =========================================================================
    // Struct-of-arrays results for batch queries
    // =========================================================================

    nb::class_<RayHits>(
        m,
        "RayHits",
        R"(Struct-of-arrays result of a batch ray cast, with one entry per input ray.

Misses are reported through the ``hit`` boolean mask; the remaining fields of a
missed ray are left at default values (invalid indices, zeros). All arrays share
the same length ``N``.)")
        .def_prop_ro(
            "hit",
            [](RayHits& self) { return view_hit(self.hit, self.n, nb::find(&self)); },
            "Boolean mask of shape (N,); ``True`` where the ray hit something.")
        .def_prop_ro(
            "mesh_index",
            [](RayHits& self) { return view_1d(self.mesh_index, self.n, nb::find(&self)); },
            "Index of the hit mesh, shape (N,).")
        .def_prop_ro(
            "instance_index",
            [](RayHits& self) { return view_1d(self.instance_index, self.n, nb::find(&self)); },
            "Index of the hit instance (relative to the source mesh), shape (N,).")
        .def_prop_ro(
            "facet_index",
            [](RayHits& self) { return view_1d(self.facet_index, self.n, nb::find(&self)); },
            "Index of the hit facet, shape (N,).")
        .def_prop_ro(
            "barycentric_coord",
            [](RayHits& self) {
                return view_2d<2>(self.barycentric_coord, self.n, nb::find(&self));
            },
            R"(Barycentric coordinates ``(u, v)`` of each hit point, shape (N, 2).
The surface point is: ``p = (1 - u - v) * p1 + u * p2 + v * p3``.)")
        .def_prop_ro(
            "position",
            [](RayHits& self) { return view_2d<3>(self.position, self.n, nb::find(&self)); },
            "World-space hit positions, shape (N, 3).")
        .def_prop_ro(
            "ray_depth",
            [](RayHits& self) { return view_1d(self.ray_depth, self.n, nb::find(&self)); },
            "Parametric distance along each ray (t value), shape (N,).")
        .def_prop_ro(
            "normal",
            [](RayHits& self) { return view_2d<3>(self.normal, self.n, nb::find(&self)); },
            "Unnormalized geometric normals at the hit points, shape (N, 3).");

    nb::class_<ClosestPointHits>(
        m,
        "ClosestPointHits",
        R"(Struct-of-arrays result of a batch closest-point query, one entry per query.

The ``hit`` boolean mask reports which queries found a closest point; the
remaining fields of a missed query are left at default values. All arrays share
the same length ``N``.)")
        .def_prop_ro(
            "hit",
            [](ClosestPointHits& self) { return view_hit(self.hit, self.n, nb::find(&self)); },
            "Boolean mask of shape (N,); ``True`` where a closest point was found.")
        .def_prop_ro(
            "mesh_index",
            [](ClosestPointHits& self) {
                return view_1d(self.mesh_index, self.n, nb::find(&self));
            },
            "Index of the hit mesh, shape (N,).")
        .def_prop_ro(
            "instance_index",
            [](ClosestPointHits& self) {
                return view_1d(self.instance_index, self.n, nb::find(&self));
            },
            "Index of the hit instance (relative to the source mesh), shape (N,).")
        .def_prop_ro(
            "facet_index",
            [](ClosestPointHits& self) {
                return view_1d(self.facet_index, self.n, nb::find(&self));
            },
            "Index of the hit facet, shape (N,).")
        .def_prop_ro(
            "barycentric_coord",
            [](ClosestPointHits& self) {
                return view_2d<2>(self.barycentric_coord, self.n, nb::find(&self));
            },
            R"(Barycentric coordinates ``(u, v)`` of each closest point, shape (N, 2).
The surface point is: ``p = (1 - u - v) * p1 + u * p2 + v * p3``.)")
        .def_prop_ro(
            "position",
            [](ClosestPointHits& self) {
                return view_2d<3>(self.position, self.n, nb::find(&self));
            },
            "World-space closest-point positions, shape (N, 3).")
        .def_prop_ro(
            "distance",
            [](ClosestPointHits& self) { return view_1d(self.distance, self.n, nb::find(&self)); },
            "Distance from each query point to its closest surface point, shape (N,).");

    // =========================================================================
    // RayCaster class  (construction and scene population only)
    // =========================================================================

    nb::class_<raycasting::RayCaster>(
        m,
        "RayCaster",
        R"(A ray caster built on top of Embree.

This class manages an Embree BVH scene for efficient spatial queries.  In the
Python API it is exposed purely as a *caching* object: build the scene once,
then pass it to the various ``project_*`` functions to avoid rebuilding the BVH
on every call.

Example::

    caster = lagrange.raycasting.RayCaster()
    caster.add_mesh(source)
    caster.commit_updates()

    lagrange.raycasting.project_closest_point(
        source, target, attribute_ids=[attr_id], project_vertices=False,
        ray_caster=caster)
    lagrange.raycasting.project_closest_vertex(
        source, target, attribute_ids=[attr_id], project_vertices=False,
        ray_caster=caster)
)")
        .def(
            "__init__",
            [](raycasting::RayCaster* self,
               int scene_flags,
               raycasting::BuildQuality build_quality) {
                new (self) raycasting::RayCaster(
                    BitField<raycasting::SceneFlags>(
                        static_cast<std::underlying_type_t<raycasting::SceneFlags>>(scene_flags)),
                    build_quality);
            },
            "scene_flags"_a = static_cast<int>(raycasting::SceneFlags::Robust),
            "build_quality"_a = raycasting::BuildQuality::Medium,
            R"(Construct a RayCaster.

:param scene_flags: Embree scene flags (default: ``SceneFlags.Robust``).
:param build_quality: BVH build quality (default: ``BuildQuality.Medium``).)")

        .def(
            "add_mesh",
            [](raycasting::RayCaster& self,
               MeshType mesh,
               std::optional<Eigen::Matrix4f> transform_matrix) -> uint32_t {
                using Affine = Eigen::Transform<Scalar, 3, Eigen::Affine>;
                if (transform_matrix) {
                    Affine t;
                    t.matrix() = transform_matrix->template cast<Scalar>();
                    return self.add_mesh(std::move(mesh), std::optional<Affine>(t));
                } else {
                    return self.add_mesh(std::move(mesh), std::optional<Affine>(std::nullopt));
                }
            },
            "mesh"_a,
            "transform"_a.none() = Eigen::Matrix4f(Eigen::Matrix4f::Identity()),
            R"(Add a triangle mesh to the scene.

The mesh is moved into the ray caster.  Call :meth:`commit_updates` after
adding all meshes.

By default a single instance with an identity transform is created.  Pass
``None`` to add the mesh without any instance (use :meth:`add_instance` to
create instances later).

:param mesh:      Triangle mesh.
:param transform: 4×4 affine transformation matrix (float32), or ``None``
                  to add the mesh without creating an instance.
:return: Index of the source mesh in the scene.)")

        .def(
            "add_scene",
            [](raycasting::RayCaster& self, scene::SimpleScene<Scalar, Index, 3> simple_scene) {
                self.add_scene(std::move(simple_scene));
            },
            "simple_scene"_a,
            R"(Add all meshes and instances from a SimpleScene.

:param simple_scene: Scene containing meshes and their instances.)")

        .def(
            "commit_updates",
            &raycasting::RayCaster::commit_updates,
            R"(Rebuild the BVH after adding or modifying meshes.

Must be called before any query or project function.)")

        .def(
            "add_instance",
            [](raycasting::RayCaster& self,
               uint32_t mesh_index,
               const Eigen::Matrix4f& transform_matrix) -> uint32_t {
                Eigen::Affine3f t;
                t.matrix() = transform_matrix;
                return self.add_instance(mesh_index, t);
            },
            "mesh_index"_a,
            "transform"_a,
            R"(Add an instance of an existing mesh with a given transform.

:param mesh_index: Index of the source mesh (returned by :meth:`add_mesh`).
:param transform:  4×4 affine transformation matrix (float32).
:return: Local instance index relative to the source mesh.)")

        .def(
            "update_mesh",
            [](raycasting::RayCaster& self, uint32_t mesh_index, const MeshType& mesh) {
                self.update_mesh(mesh_index, mesh);
            },
            "mesh_index"_a,
            "mesh"_a,
            R"(Replace a mesh in the scene.

All instances of the old mesh will reference the new mesh.

:param mesh_index: Index of the mesh to replace.
:param mesh:       New triangle mesh.)")
        .def(
            "update_vertices",
            [](raycasting::RayCaster& self, uint32_t mesh_index, const MeshType& mesh) {
                self.update_vertices(mesh_index, mesh);
            },
            "mesh_index"_a,
            "mesh"_a,
            R"(Notify that vertices of a mesh have been modified.

The number and order of vertices must not change.

:param mesh_index: Index of the mesh whose vertices changed.
:param mesh:       The modified mesh with updated vertex positions.)")
        .def(
            "get_transform",
            [](const raycasting::RayCaster& self,
               uint32_t mesh_index,
               uint32_t instance_index) -> Eigen::Matrix4f {
                return self.get_transform(mesh_index, instance_index).matrix();
            },
            "mesh_index"_a,
            "instance_index"_a,
            R"(Get the affine transform of a mesh instance.

:param mesh_index:     Index of the source mesh.
:param instance_index: Local instance index.
:return: 4×4 affine transformation matrix (float32).)")

        .def(
            "update_transform",
            [](raycasting::RayCaster& self,
               uint32_t mesh_index,
               uint32_t instance_index,
               const Eigen::Matrix4f& transform_matrix) {
                Eigen::Affine3f t;
                t.matrix() = transform_matrix;
                self.update_transform(mesh_index, instance_index, t);
            },
            "mesh_index"_a,
            "instance_index"_a,
            "transform"_a,
            R"(Update the affine transform of a mesh instance.

:param mesh_index:     Index of the source mesh.
:param instance_index: Local instance index.
:param transform:      New 4×4 affine transformation matrix (float32).)")

        .def(
            "get_visibility",
            &raycasting::RayCaster::get_visibility,
            "mesh_index"_a,
            "instance_index"_a,
            R"(Get the visibility flag of a mesh instance.

:param mesh_index:     Index of the source mesh.
:param instance_index: Local instance index.
:return: True if the instance is visible.)")

        .def(
            "update_visibility",
            &raycasting::RayCaster::update_visibility,
            "mesh_index"_a,
            "instance_index"_a,
            "visible"_a,
            R"(Update the visibility of a mesh instance.

:param mesh_index:     Index of the source mesh.
:param instance_index: Local instance index.
:param visible:        True to make visible, False to hide.)")

        .def(
            "cast",
            [](const raycasting::RayCaster& caster,
               FloatArray origins,
               FloatArray directions,
               FloatParam tmin_param,
               FloatParam tmax_param) -> std::variant<std::optional<raycasting::RayHit>, RayHits> {
                if (origins.ndim() != directions.ndim()) {
                    throw std::invalid_argument(
                        "origins and directions must have the same number of dimensions "
                        "(both (3,) or both (N, 3))");
                }
                size_t n_origins = get_num_points(origins, "origins");
                size_t n_directions = get_num_points(directions, "directions");
                if (n_origins != n_directions) {
                    throw std::invalid_argument(
                        "origins and directions must have the same number of rays");
                }
                size_t n = n_origins;

                // Backing storage for broadcast (None/scalar) tmin/tmax maps;
                // must outlive the maps and the parallel region below.
                float tmin_scratch = 0.0f, tmax_scratch = 0.0f;
                const auto tmin_vals = view_float_param(tmin_param, n, 0.0f, "tmin", tmin_scratch);
                const auto tmax_vals = view_float_param(
                    tmax_param,
                    n,
                    std::numeric_limits<float>::infinity(),
                    "tmax",
                    tmax_scratch);

                // Return type follows input dimensionality:
                // - 1D (3,)  -> single RayHit | None
                // - 2D (N,3) -> RayHits struct-of-arrays, one entry per ray (even for N == 1).
                if (origins.ndim() == 1) {
                    return caster.cast(
                        get_row(origins, 0),
                        get_row(directions, 0),
                        tmin_vals[0],
                        tmax_vals[0]);
                }

                // Convert one entry of a RayHitN packet to an optional RayHit.
                auto to_hit = [](const auto& hits, size_t i) -> std::optional<raycasting::RayHit> {
                    if (!hits.is_valid(i)) return std::nullopt;
                    raycasting::RayHit h;
                    h.mesh_index = hits.mesh_indices[i];
                    h.instance_index = hits.instance_indices[i];
                    h.facet_index = hits.facet_indices[i];
                    h.barycentric_coord = hits.barycentric_coords.col(i);
                    h.position = hits.positions.col(i);
                    h.ray_depth = hits.ray_depths[i];
                    h.normal = hits.normals.col(i);
                    return h;
                };

                RayHits out;
                out.resize(n);

                // Process rays in parallel over chunks of up to 16, picking the packet
                // width (16, 8, 4, or a single-ray query) that best fits each chunk. The
                // GIL is released for the parallel region: results are written straight
                // into the struct-of-arrays (pure C++, no per-ray Python objects).
                const size_t num_chunks = (n + 15) / 16;
                {
                    nb::gil_scoped_release release;
                    tbb::parallel_for(size_t(0), num_chunks, [&](size_t chunk) {
                        const size_t offset = chunk * 16;
                        const size_t count = std::min(n - offset, size_t(16));

                        auto cast_batch = [&](auto point_tag, auto float_tag, size_t N) {
                            using PointNf = decltype(point_tag);
                            using FloatN = decltype(float_tag);
                            PointNf orig, dir;
                            FloatN tmin_v, tmax_v;
                            for (size_t i = 0; i < count; ++i) {
                                orig.row(i) = get_row(origins, offset + i).transpose();
                                dir.row(i) = get_row(directions, offset + i).transpose();
                                tmin_v[i] = tmin_vals[offset + i];
                                tmax_v[i] = tmax_vals[offset + i];
                            }
                            for (size_t i = count; i < N; ++i) {
                                orig.row(i).setZero();
                                dir.row(i).setZero();
                                tmin_v[i] = 0.0f;
                                tmax_v[i] = std::numeric_limits<float>::infinity();
                            }
                            if constexpr (PointNf::RowsAtCompileTime == 4) {
                                return caster.cast4(orig, dir, count, tmin_v, tmax_v);
                            } else if constexpr (PointNf::RowsAtCompileTime == 8) {
                                return caster.cast8(orig, dir, count, tmin_v, tmax_v);
                            } else {
                                return caster.cast16(orig, dir, count, tmin_v, tmax_v);
                            }
                        };

                        if (count == 1) {
                            if (auto h = caster.cast(
                                    get_row(origins, offset),
                                    get_row(directions, offset),
                                    tmin_vals[offset],
                                    tmax_vals[offset]))
                                out.set(offset, *h);
                        } else if (count <= 4) {
                            auto hits = cast_batch(
                                raycasting::RayCaster::Point4f{},
                                raycasting::RayCaster::Float4{},
                                4);
                            for (size_t i = 0; i < count; ++i)
                                if (auto h = to_hit(hits, i)) out.set(offset + i, *h);
                        } else if (count <= 8) {
                            auto hits = cast_batch(
                                raycasting::RayCaster::Point8f{},
                                raycasting::RayCaster::Float8{},
                                8);
                            for (size_t i = 0; i < count; ++i)
                                if (auto h = to_hit(hits, i)) out.set(offset + i, *h);
                        } else {
                            auto hits = cast_batch(
                                raycasting::RayCaster::Point16f{},
                                raycasting::RayCaster::Float16{},
                                16);
                            for (size_t i = 0; i < count; ++i)
                                if (auto h = to_hit(hits, i)) out.set(offset + i, *h);
                        }
                    });
                }

                return out;
            },
            "origins"_a,
            "directions"_a,
            "tmin"_a = nb::none(),
            "tmax"_a = nb::none(),
            R"(Cast one or more rays and find the closest intersections.

For a single ray, pass 1D arrays of shape (3,) for origin and direction.
For batch queries, pass 2D arrays of shape (N, 3) for any N >= 1.
The appropriate SIMD packet function (cast4, cast8, cast16) is selected
automatically based on batch size, and batches are processed in parallel.
For N > 16, rays are processed in chunks of up to 16.

``origins`` and ``directions`` must have matching dimensionality (both
(3,) or both (N, 3)); mixing the two raises ``ValueError``.

:param origins:    Ray origin(s). Shape (3,) for a single ray or (N, 3) for a batch.
:param directions: Ray direction(s). Shape (3,) for a single ray or (N, 3) for a batch.
:param tmin:       Minimum parametric distance(s). Scalar or array of shape (N,). If None, default is 0.
:param tmax:       Maximum parametric distance(s). Scalar or array of shape (N,). If None, default is inf.
:return: For a single ray (1D input): a ``RayHit`` or ``None`` on a miss.
         For a batch (2D input): a ``RayHits`` struct-of-arrays with one entry per
         ray; misses are flagged by the ``hit`` boolean mask.)")

        .def(
            "closest_point",
            [](const raycasting::RayCaster& caster, FloatArray query_points)
                -> std::variant<std::optional<raycasting::ClosestPointHit>, ClosestPointHits> {
                size_t n = get_num_points(query_points, "query_points");

                // Return type follows input dimensionality:
                // - 1D (3,)  -> single ClosestPointHit | None
                // - 2D (N,3) -> ClosestPointHits struct-of-arrays, one entry per query.
                if (query_points.ndim() == 1) {
                    return caster.closest_point(get_row(query_points, 0));
                }

                // Convert one entry of a ClosestPointHitN packet to an optional hit.
                auto to_hit = [](const auto& hits,
                                 size_t i) -> std::optional<raycasting::ClosestPointHit> {
                    if (!hits.is_valid(i)) return std::nullopt;
                    raycasting::ClosestPointHit h;
                    h.mesh_index = hits.mesh_indices[i];
                    h.instance_index = hits.instance_indices[i];
                    h.facet_index = hits.facet_indices[i];
                    h.barycentric_coord = hits.barycentric_coords.col(i);
                    h.position = hits.positions.col(i);
                    h.distance = hits.distances[i];
                    return h;
                };

                ClosestPointHits out;
                out.resize(n);

                // Process queries in parallel over chunks of up to 16, picking the packet
                // width (16, 8, 4, or a single query) that best fits each chunk. The GIL
                // is released for the parallel region: results are written straight into
                // the struct-of-arrays (pure C++, no per-query Python objects).
                const size_t num_chunks = (n + 15) / 16;
                {
                    nb::gil_scoped_release release;
                    tbb::parallel_for(size_t(0), num_chunks, [&](size_t chunk) {
                        const size_t offset = chunk * 16;
                        const size_t count = std::min(n - offset, size_t(16));

                        auto query_batch = [&](auto point_tag, size_t N) {
                            using PointNf = decltype(point_tag);
                            PointNf pts;
                            for (size_t i = 0; i < count; ++i) {
                                pts.row(i) = get_row(query_points, offset + i).transpose();
                            }
                            for (size_t i = count; i < N; ++i) {
                                pts.row(i).setZero();
                            }
                            if constexpr (PointNf::RowsAtCompileTime == 4) {
                                return caster.closest_point4(pts, count);
                            } else if constexpr (PointNf::RowsAtCompileTime == 8) {
                                return caster.closest_point8(pts, count);
                            } else {
                                return caster.closest_point16(pts, count);
                            }
                        };

                        if (count == 1) {
                            if (auto h = caster.closest_point(get_row(query_points, offset)))
                                out.set(offset, *h);
                        } else if (count <= 4) {
                            auto hits = query_batch(raycasting::RayCaster::Point4f{}, 4);
                            for (size_t i = 0; i < count; ++i)
                                if (auto h = to_hit(hits, i)) out.set(offset + i, *h);
                        } else if (count <= 8) {
                            auto hits = query_batch(raycasting::RayCaster::Point8f{}, 8);
                            for (size_t i = 0; i < count; ++i)
                                if (auto h = to_hit(hits, i)) out.set(offset + i, *h);
                        } else {
                            auto hits = query_batch(raycasting::RayCaster::Point16f{}, 16);
                            for (size_t i = 0; i < count; ++i)
                                if (auto h = to_hit(hits, i)) out.set(offset + i, *h);
                        }
                    });
                }

                return out;
            },
            "query_points"_a,
            R"(Find the closest point on the scene for one or more query points.

For a single query, pass a 1D array of shape (3,).
For batch queries, pass a 2D array of shape (N, 3) for any N >= 1.
The appropriate SIMD packet function (closest_point4, closest_point8,
closest_point16) is selected automatically based on batch size, and
batches are processed in parallel. For N > 16, queries are processed in
chunks of up to 16.

:param query_points: Query point(s). Shape (3,) for a single point or (N, 3) for a batch.
:return: For a single query (1D input): a ``ClosestPointHit`` or ``None`` on a miss.
         For a batch (2D input): a ``ClosestPointHits`` struct-of-arrays with one
         entry per query; misses are flagged by the ``hit`` boolean mask.)")

        .def(
            "occluded",
            [](const raycasting::RayCaster& caster,
               FloatArray origins,
               FloatArray directions,
               FloatParam tmin_param,
               FloatParam tmax_param) -> std::variant<bool, BoolArray> {
                if (origins.ndim() != directions.ndim()) {
                    throw std::invalid_argument(
                        "origins and directions must have the same number of dimensions "
                        "(both (3,) or both (N, 3))");
                }
                size_t n_origins = get_num_points(origins, "origins");
                size_t n_directions = get_num_points(directions, "directions");
                if (n_origins != n_directions) {
                    throw std::invalid_argument(
                        "origins and directions must have the same number of rays");
                }
                size_t n = n_origins;

                // Backing storage for broadcast (None/scalar) tmin/tmax maps;
                // must outlive the maps and the parallel region below.
                float tmin_scratch = 0.0f, tmax_scratch = 0.0f;
                const auto tmin_vals = view_float_param(tmin_param, n, 0.0f, "tmin", tmin_scratch);
                const auto tmax_vals = view_float_param(
                    tmax_param,
                    n,
                    std::numeric_limits<float>::infinity(),
                    "tmax",
                    tmax_scratch);

                // Return type follows input dimensionality:
                // - 1D (3,)  -> single bool
                // - 2D (N,3) -> list of bool, one per ray (even for N == 1).
                if (origins.ndim() == 1) {
                    return caster.occluded(
                        get_row(origins, 0),
                        get_row(directions, 0),
                        tmin_vals[0],
                        tmax_vals[0]);
                }

                // 0/1 byte per ray; std::vector<bool> is unsafe for concurrent writes.
                std::vector<uint8_t> results(n, 0);

                // Process rays in parallel over chunks of up to 16, picking the packet
                // width (16, 8, 4, or a single-ray query) that best fits each chunk. The
                // GIL is released for the parallel region (pure C++, no Python access);
                // the result is wrapped in a numpy array afterwards.
                const size_t num_chunks = (n + 15) / 16;
                {
                    nb::gil_scoped_release release;
                    tbb::parallel_for(size_t(0), num_chunks, [&](size_t chunk) {
                        const size_t offset = chunk * 16;
                        const size_t count = std::min(n - offset, size_t(16));

                        auto occluded_batch = [&](auto point_tag, auto float_tag, size_t N) {
                            using PointNf = decltype(point_tag);
                            using FloatN = decltype(float_tag);
                            PointNf orig, dir;
                            FloatN tmin_v, tmax_v;
                            for (size_t i = 0; i < count; ++i) {
                                orig.row(i) = get_row(origins, offset + i).transpose();
                                dir.row(i) = get_row(directions, offset + i).transpose();
                                tmin_v[i] = tmin_vals[offset + i];
                                tmax_v[i] = tmax_vals[offset + i];
                            }
                            for (size_t i = count; i < N; ++i) {
                                orig.row(i).setZero();
                                dir.row(i).setZero();
                                tmin_v[i] = 0.0f;
                                tmax_v[i] = std::numeric_limits<float>::infinity();
                            }
                            if constexpr (PointNf::RowsAtCompileTime == 4) {
                                return caster.occluded4(orig, dir, count, tmin_v, tmax_v);
                            } else if constexpr (PointNf::RowsAtCompileTime == 8) {
                                return caster.occluded8(orig, dir, count, tmin_v, tmax_v);
                            } else {
                                return caster.occluded16(orig, dir, count, tmin_v, tmax_v);
                            }
                        };

                        if (count == 1) {
                            results[offset] = caster.occluded(
                                                  get_row(origins, offset),
                                                  get_row(directions, offset),
                                                  tmin_vals[offset],
                                                  tmax_vals[offset])
                                                  ? 1u
                                                  : 0u;
                            return;
                        }

                        uint32_t mask = 0;
                        if (count <= 4) {
                            mask = occluded_batch(
                                raycasting::RayCaster::Point4f{},
                                raycasting::RayCaster::Float4{},
                                4);
                        } else if (count <= 8) {
                            mask = occluded_batch(
                                raycasting::RayCaster::Point8f{},
                                raycasting::RayCaster::Float8{},
                                8);
                        } else {
                            mask = occluded_batch(
                                raycasting::RayCaster::Point16f{},
                                raycasting::RayCaster::Float16{},
                                16);
                        }
                        for (size_t i = 0; i < count; ++i) {
                            results[offset + i] = (mask & (1u << i)) != 0 ? 1u : 0u;
                        }
                    });
                }

                // Hand ownership of the buffer to a capsule so the returned numpy
                // array can view it directly (no copy, no per-ray Python objects).
                auto* buffer = new std::vector<uint8_t>(std::move(results));
                nb::capsule owner(buffer, [](void* p) noexcept {
                    delete static_cast<std::vector<uint8_t>*>(p);
                });
                return BoolArray(reinterpret_cast<bool*>(buffer->data()), {n}, owner);
            },
            "origins"_a,
            "directions"_a,
            "tmin"_a = nb::none(),
            "tmax"_a = nb::none(),
            R"(Test whether one or more rays are occluded (hit anything).

For a single ray, pass 1D arrays of shape (3,) for origin and direction.
For batch queries, pass 2D arrays of shape (N, 3) for any N >= 1.
The appropriate SIMD packet function (occluded4, occluded8, occluded16) is
selected automatically based on batch size, and batches are processed in
parallel. For N > 16, rays are processed in chunks of up to 16.

``origins`` and ``directions`` must have matching dimensionality (both
(3,) or both (N, 3)); mixing the two raises ``ValueError``.

:param origins:    Ray origin(s). Shape (3,) for a single ray or (N, 3) for a batch.
:param directions: Ray direction(s). Shape (3,) for a single ray or (N, 3) for a batch.
:param tmin:       Minimum parametric distance(s). Scalar or array of shape (N,). If None, default is 0.
:param tmax:       Maximum parametric distance(s). Scalar or array of shape (N,). If None, default is inf.
:return: For a single ray (1D input): a ``bool``.
         For a batch (2D input): a numpy ``bool`` array of shape (N,), one per ray.)");

    // =========================================================================
    // project_attributes  (combined convenience function)
    // =========================================================================

    m.def(
        "project",
        [](const MeshType& source,
           MeshType& target,
           std::vector<AttributeId> attribute_ids,
           bool project_vertices,
           raycasting::ProjectMode project_mode,
           DirectionParam direction,
           raycasting::CastMode cast_mode,
           raycasting::FallbackMode fallback_mode,
           double default_value,
           const raycasting::RayCaster* ray_caster) {
            raycasting::ProjectOptions opts;
            opts.attribute_ids = std::move(attribute_ids);
            opts.project_vertices = project_vertices;
            opts.project_mode = project_mode;
            std::visit(
                [&](const auto& v) {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, std::monostate>) {
                        opts.direction = std::monostate{};
                    } else {
                        opts.direction = v;
                    }
                },
                direction);
            opts.cast_mode = cast_mode;
            opts.fallback_mode = fallback_mode;
            opts.default_value = default_value;
            raycasting::project(source, target, opts, ray_caster);
        },
        "source"_a,
        "target"_a,
        "attribute_ids"_a = std::vector<AttributeId>{},
        "project_vertices"_a = true,
        "project_mode"_a = raycasting::ProjectMode::ClosestPoint,
        "direction"_a = nb::none(),
        "cast_mode"_a = raycasting::CastMode::BothWays,
        "fallback_mode"_a = raycasting::FallbackMode::Constant,
        "default_value"_a = 0.0,
        "ray_caster"_a = nullptr,
        R"(Project vertex attributes from one mesh to another.

:param source:          Source triangle mesh.
:param target:          Target mesh (modified in place).
:param attribute_ids:   List of additional source vertex attribute ids to transfer.
:param project_vertices: If True (default), vertex positions are automatically projected.
:param project_mode:    Projection mode (default: ``ProjectMode.ClosestPoint``).
:param direction:       Ray direction. Can be None (default, uses vertex normals), a 3D vector,
                        or an AttributeId for a per-vertex direction attribute on the target mesh.
                        Only used with ``ProjectMode.RayCasting``.
:param cast_mode:       Forward-only or both directions (only used with ``ProjectMode.RayCasting``).
:param fallback_mode:   Fallback for missed vertices (only used with ``ProjectMode.RayCasting``).
:param default_value:   Fill value for ``FallbackMode.Constant`` (default: 0).
:param ray_caster:      Optional pre-built :class:`RayCaster` for caching.)");

    // =========================================================================
    // project_attributes_directional
    // =========================================================================

    m.def(
        "project_directional",
        [](const MeshType& source,
           MeshType& target,
           std::vector<AttributeId> attribute_ids,
           bool project_vertices,
           DirectionParam direction,
           raycasting::CastMode cast_mode,
           raycasting::FallbackMode fallback_mode,
           double default_value,
           const raycasting::RayCaster* ray_caster) {
            raycasting::ProjectDirectionalOptions opts;
            opts.attribute_ids = std::move(attribute_ids);
            opts.project_vertices = project_vertices;
            std::visit(
                [&](const auto& v) {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, std::monostate>) {
                        opts.direction = std::monostate{};
                    } else {
                        opts.direction = v;
                    }
                },
                direction);
            opts.cast_mode = cast_mode;
            opts.fallback_mode = fallback_mode;
            opts.default_value = default_value;
            raycasting::project_directional(source, target, opts, ray_caster);
        },
        "source"_a,
        "target"_a,
        "attribute_ids"_a = std::vector<AttributeId>{},
        "project_vertices"_a = true,
        "direction"_a = nb::none(),
        "cast_mode"_a = raycasting::CastMode::BothWays,
        "fallback_mode"_a = raycasting::FallbackMode::Constant,
        "default_value"_a = 0.0,
        "ray_caster"_a = nullptr,
        R"(Project vertex attributes along a prescribed direction.

For each target vertex, a ray is cast in the given direction.  If it hits the
source mesh, attribute values are interpolated from the hit triangle.

:param source:          Source triangle mesh.
:param target:          Target mesh (modified in place).
:param attribute_ids:   List of additional source vertex attribute ids to transfer.
:param project_vertices: If True (default), vertex positions are automatically projected.
:param direction:       Ray direction. Can be None (default, uses vertex normals), a 3D vector,
                        or an AttributeId for a per-vertex direction attribute on the target mesh.
:param cast_mode:       Forward-only or both directions (default: ``CastMode.BothWays``).
:param fallback_mode:   Fallback for missed vertices (default: ``FallbackMode.Constant``).
:param default_value:   Fill value for ``FallbackMode.Constant`` (default: 0).
:param ray_caster:      Optional pre-built :class:`RayCaster` for caching.)");

    // =========================================================================
    // project_attributes_closest_point
    // =========================================================================

    m.def(
        "project_closest_point",
        [](const MeshType& source,
           MeshType& target,
           std::vector<AttributeId> attribute_ids,
           bool project_vertices,
           const raycasting::RayCaster* ray_caster) {
            raycasting::ProjectCommonOptions opts;
            opts.attribute_ids = std::move(attribute_ids);
            opts.project_vertices = project_vertices;
            raycasting::project_closest_point(source, target, opts, ray_caster);
        },
        "source"_a,
        "target"_a,
        "attribute_ids"_a = std::vector<AttributeId>{},
        "project_vertices"_a = true,
        "ray_caster"_a = nullptr,
        R"(Project vertex attributes by closest-point interpolation.

For each target vertex, the closest point on the source mesh surface is found
and attribute values are linearly interpolated from the face corners.

:param source:          Source triangle mesh.
:param target:          Target mesh (modified in place).
:param attribute_ids:   List of additional source vertex attribute ids to transfer.
:param project_vertices: If True (default), vertex positions are automatically projected.
:param ray_caster:      Optional pre-built :class:`RayCaster` for caching.)");

    // =========================================================================
    // project_attributes_closest_vertex
    // =========================================================================

    m.def(
        "project_closest_vertex",
        [](const MeshType& source,
           MeshType& target,
           std::vector<AttributeId> attribute_ids,
           bool project_vertices,
           const raycasting::RayCaster* ray_caster) {
            raycasting::ProjectCommonOptions opts;
            opts.attribute_ids = std::move(attribute_ids);
            opts.project_vertices = project_vertices;
            raycasting::project_closest_vertex(source, target, opts, ray_caster);
        },
        "source"_a,
        "target"_a,
        "attribute_ids"_a = std::vector<AttributeId>{},
        "project_vertices"_a = true,
        "ray_caster"_a = nullptr,
        R"(Project vertex attributes by closest-vertex snapping.

For each target vertex, the closest surface point is found and snapped to the
nearest vertex of the hit triangle.  Attribute values are copied directly from
that source vertex (no interpolation).

:param source:          Source triangle mesh.
:param target:          Target mesh (modified in place).
:param attribute_ids:   List of additional source vertex attribute ids to transfer.
:param project_vertices: If True (default), vertex positions are automatically projected.
:param ray_caster:      Optional pre-built :class:`RayCaster` for caching.)");

    // =========================================================================
    // NumPy-array overloads
    //
    // Instead of a SurfaceMesh target, accept an (N,3) NumPy array of query points (and
    // optionally directions). A temporary SurfaceMesh is created by wrapping the input buffer
    // via wrap_as_vertices (zero-copy). The input buffer is modified in place and returned by
    // value.
    // =========================================================================

    // ----- project_closest_point (NumPy overload) ----------------------------

    m.def(
        "project_closest_point",
        [](const MeshType& source,
           NDArray3D points,
           const raycasting::RayCaster* ray_caster) -> NDArray3D {
            auto target = make_point_mesh(points);
            raycasting::ProjectCommonOptions opts;
            opts.project_vertices = true;
            raycasting::project_closest_point(source, target, opts, ray_caster);
            return points;
        },
        "source"_a,
        "points"_a,
        "ray_caster"_a = nullptr,
        R"(Project query points onto a source mesh by closest-point interpolation.

For each query point, the closest point on the source mesh surface is found.

:param source:          Source triangle mesh.
:param points:          (N, 3) NumPy array of query point positions (float64). Modified in place to store the projected positions.
:param ray_caster:      Optional pre-built :class:`RayCaster` for caching.
:return: (N, 3) NumPy array of projected positions (float64).)");

    // ----- project_closest_vertex (NumPy overload) ---------------------------

    m.def(
        "project_closest_vertex",
        [](const MeshType& source,
           NDArray3D points,
           const raycasting::RayCaster* ray_caster) -> NDArray3D {
            auto target = make_point_mesh(points);
            raycasting::ProjectCommonOptions opts;
            opts.project_vertices = true;
            raycasting::project_closest_vertex(source, target, opts, ray_caster);
            return points;
        },
        "source"_a,
        "points"_a,
        "ray_caster"_a = nullptr,
        R"(Project query points onto a source mesh by closest-vertex snapping.

For each query point, the closest surface point is found and snapped to the
nearest source vertex.

:param source:          Source triangle mesh.
:param points:          (N, 3) NumPy array of query point positions (float64). Modified in place to store the projected positions.
:param ray_caster:      Optional pre-built :class:`RayCaster` for caching.
:return: (N, 3) NumPy array of projected positions (float64).)");

    // ----- project_directional (NumPy overload) ------------------------------

    m.def(
        "project_directional",
        [](const MeshType& source,
           NDArray3D points,
           std::variant<Eigen::Vector3f, NDArray3D> direction,
           raycasting::CastMode cast_mode,
           raycasting::FallbackMode fallback_mode,
           double default_value,
           const raycasting::RayCaster* ray_caster) -> NDArray3D {
            auto target = make_point_mesh(points);
            raycasting::ProjectDirectionalOptions opts;
            opts.cast_mode = cast_mode;
            opts.fallback_mode = fallback_mode;
            opts.default_value = default_value;
            opts.direction = resolve_direction(direction, target);
            raycasting::project_directional(source, target, opts, ray_caster);
            return points;
        },
        "source"_a,
        "points"_a,
        "direction"_a,
        "cast_mode"_a = raycasting::ProjectDirectionalOptions().cast_mode,
        "fallback_mode"_a = raycasting::ProjectDirectionalOptions().fallback_mode,
        "default_value"_a = raycasting::ProjectDirectionalOptions().default_value,
        "ray_caster"_a = nullptr,
        R"(Project query points onto a source mesh along a prescribed direction.

For each query point, a ray is cast in the given direction.  If it hits the
source mesh, vertex positions are set from the hit.

:param source:          Source triangle mesh.
:param points:          (N, 3) NumPy array of query point positions (float64). Modified in place to store the projected positions.
:param direction:       Ray direction. Can be a 3D vector, or a numpy array of shape (N, 3) for per-vertex directions,
:param cast_mode:       Forward-only or both directions (default: ``CastMode.BothWays``).
:param fallback_mode:   Fallback for missed vertices (default: ``FallbackMode.Constant``).
:param default_value:   Fill value for ``FallbackMode.Constant`` (default: 0).
:param ray_caster:      Optional pre-built :class:`RayCaster` for caching.
:return: (N, 3) NumPy array of projected positions (float64).)");

    // ----- project (NumPy overload) ------------------------------------------

    m.def(
        "project",
        [](const MeshType& source,
           NDArray3D points,
           raycasting::ProjectMode project_mode,
           std::optional<std::variant<Eigen::Vector3f, NDArray3D>> direction,
           raycasting::CastMode cast_mode,
           raycasting::FallbackMode fallback_mode,
           double default_value,
           const raycasting::RayCaster* ray_caster) -> NDArray3D {
            auto target = make_point_mesh(points);
            raycasting::ProjectOptions opts;
            opts.project_vertices = true;
            opts.project_mode = project_mode;
            opts.cast_mode = cast_mode;
            opts.fallback_mode = fallback_mode;
            opts.default_value = default_value;
            if (direction.has_value()) {
                opts.direction = resolve_direction(direction.value(), target);
            }
            raycasting::project(source, target, opts, ray_caster);
            return points;
        },
        "source"_a,
        "points"_a,
        "project_mode"_a,
        "direction"_a = nb::none(),
        "cast_mode"_a = raycasting::ProjectOptions().cast_mode,
        "fallback_mode"_a = raycasting::ProjectOptions().fallback_mode,
        "default_value"_a = raycasting::ProjectOptions().default_value,
        "ray_caster"_a = nullptr,
        R"(Project query points onto a source mesh.

:param source:          Source triangle mesh.
:param points:          (N, 3) NumPy array of query point positions (float64). Modified in place to store the projected positions.
:param project_mode:    Projection mode (default: ``ProjectMode.ClosestPoint``).
:param direction:       Ray direction. Can be a 3D vector, or a numpy array of shape (N, 3) for per-vertex directions.
                        Only used with ``ProjectMode.RayCasting``.
:param cast_mode:       Forward-only or both directions (only used with ``ProjectMode.RayCasting``).
:param fallback_mode:   Fallback for missed vertices (only used with ``ProjectMode.RayCasting``).
:param default_value:   Fill value for ``FallbackMode.Constant`` (default: 0).
:param ray_caster:      Optional pre-built :class:`RayCaster` for caching.

:return: (N, 3) NumPy array of projected positions (float64).)");

    // =========================================================================
    // compute_local_feature_size
    // =========================================================================

    m.def(
        "compute_local_feature_size",
        [](MeshType& mesh,
           std::string_view output_attribute_name,
           std::string_view direction_mode,
           float ray_offset,
           float default_lfs,
           float medial_axis_tolerance,
           const raycasting::RayCaster* ray_caster) -> AttributeId {
            raycasting::LocalFeatureSizeOptions opts;
            opts.output_attribute_name = output_attribute_name;

            if (direction_mode == "interior") {
                opts.direction_mode = raycasting::RayDirectionMode::Interior;
            } else if (direction_mode == "exterior") {
                opts.direction_mode = raycasting::RayDirectionMode::Exterior;
            } else if (direction_mode == "both") {
                opts.direction_mode = raycasting::RayDirectionMode::Both;
            } else {
                throw std::runtime_error(
                    "Invalid direction_mode. Use 'interior', 'exterior', or 'both'.");
            }

            opts.ray_offset = ray_offset;
            opts.default_lfs = default_lfs;
            opts.medial_axis_tolerance = medial_axis_tolerance;
            return raycasting::compute_local_feature_size(mesh, opts, ray_caster);
        },
        "mesh"_a,
        nb::kw_only(),
        "output_attribute_name"_a = "@lfs",
        "direction_mode"_a = "interior",
        "ray_offset"_a = 1e-4f,
        "default_lfs"_a = std::numeric_limits<float>::infinity(),
        "medial_axis_tolerance"_a = 1e-4f,
        "ray_caster"_a = nullptr,
        R"(Compute local feature size for each vertex using medial axis approximation.

The local feature size is stored as a per-vertex attribute on the mesh.

:param mesh:                    Triangle mesh (modified in place to add the LFS attribute).
:param output_attribute_name:   Name of the output LFS attribute (default: ``"@lfs"``).
:param direction_mode:          Ray direction mode -- ``"interior"``, ``"exterior"``, or
                                ``"both"`` (default: ``"interior"``).
:param ray_offset:              Ray offset along the vertex normal to avoid self-intersection
                                (relative to bounding box diagonal). The actual offset distance
                                is ``ray_offset * bbox_diagonal`` (default: 1e-4).
:param default_lfs:             Default local feature size value used when raycasting fails
                                to find valid hits (default: infinity).
:param medial_axis_tolerance:   Error tolerance for medial axis binary search convergence
                                (relative to bounding box diagonal). The binary search stops
                                when ``|distance_to_surface - depth_along_ray| < tolerance *
                                bbox_diagonal``. Smaller values produce more accurate results
                                but require more iterations (default: 1e-4).
:param ray_caster:              Optional pre-built :class:`RayCaster` for caching.
:return: Attribute id of the newly added LFS attribute.
:rtype: int)");

    // =========================================================================
    // Occluded-facet / occluded-instance samplers
    // =========================================================================

    using SimpleScene3D = scene::SimpleScene<Scalar, Index, 3>;
    using IsOccluderFn = std::function<bool(Index, Index)>;
    constexpr raycasting::OccludedFacetEstimateOptions facet_defaults{};
    constexpr raycasting::OccludedFacetSamplerOptions facet_sampler_defaults{};
    constexpr raycasting::OccludedInstanceEstimateOptions instance_defaults{};

    m.def(
        "remove_occluded_facets",
        [](const SimpleScene3D& scene,
           uint64_t num_rays,
           uint64_t batch_size,
           uint64_t num_adaptive_per_normal,
           bool brute_force,
           bool until_converged,
           double jitter_sigma,
           int visibility_threshold,
           std::optional<IsOccluderFn> is_occluder) {
            la_runtime_assert(
                visibility_threshold >= 1 && visibility_threshold <= 255,
                "visibility_threshold must be in [1, 255]");
            raycasting::RemoveOccludedFacetsOptions options;
            options.estimate_options.num_rays = num_rays;
            options.estimate_options.batch_size = batch_size;
            options.estimate_options.num_adaptive_per_normal = num_adaptive_per_normal;
            options.estimate_options.brute_force = brute_force;
            options.estimate_options.until_converged = until_converged;
            options.sampler_options.jitter_sigma = jitter_sigma;
            options.sampler_options.visibility_threshold =
                static_cast<uint8_t>(visibility_threshold);
            ProgressCallback progress;
            if (is_occluder) {
                return raycasting::remove_occluded_facets<Scalar, Index>(
                    scene,
                    options,
                    progress,
                    *is_occluder);
            }
            return raycasting::remove_occluded_facets<Scalar, Index>(scene, options, progress);
        },
        "scene"_a,
        nb::kw_only(),
        "num_rays"_a = facet_defaults.num_rays,
        "batch_size"_a = facet_defaults.batch_size,
        "num_adaptive_per_normal"_a = facet_defaults.num_adaptive_per_normal,
        "brute_force"_a = facet_defaults.brute_force,
        "until_converged"_a = facet_defaults.until_converged,
        "jitter_sigma"_a = facet_sampler_defaults.jitter_sigma,
        "visibility_threshold"_a = facet_sampler_defaults.visibility_threshold,
        "is_occluder"_a = nb::none(),
        R"(Build a new scene with facets not visible from the outside removed.

The output contains one unique mesh per input instance: instances of the same source mesh can
end up with different facets culled, so the input's instancing cannot be preserved.

:param scene:                   Input scene.
:param num_rays:                Total ray budget. Must be > 0 unless ``until_converged`` is True.
:param batch_size:              Rays per batch.
:param num_adaptive_per_normal: Adaptive batches per normal batch (0 = pure cosine sampling).
                                Ignored when ``brute_force`` is True.
:param brute_force:             Run brute-force batches only — baseline for benchmarking.
:param until_converged:         Stop early when a cycle finds no new visible facets.
:param jitter_sigma:            Std-dev of Gaussian jitter applied to adaptive seed directions.
:param visibility_threshold:    Number of independent escapes required to mark a facet visible
                                (>= 1). 1 = first-escape-wins (original); 2-3 dampens hairline-
                                gap shrapnel.
:param is_occluder:             Optional callable ``(mesh_index, instance_index) -> bool``
                                returning whether an instance should block rays. Non-occluders
                                are still tested for visibility but do not contribute to the
                                ray-caster scene. Defaults to None (every instance is an
                                occluder).
:return: Scene with occluded facets removed.)");

    m.def(
        "remove_occluded_instances",
        [](const SimpleScene3D& scene,
           uint64_t num_rays,
           uint64_t batch_size,
           bool until_converged,
           std::optional<IsOccluderFn> is_occluder) {
            raycasting::OccludedInstanceEstimateOptions options;
            options.num_rays = num_rays;
            options.batch_size = batch_size;
            options.until_converged = until_converged;
            ProgressCallback progress;
            // Explicit template args bypass deduction — function_ref is constructed from
            // std::function via the implicit conversion only after deduction is settled.
            if (is_occluder) {
                return raycasting::remove_occluded_instances<Scalar, Index>(
                    scene,
                    options,
                    progress,
                    *is_occluder);
            }
            return raycasting::remove_occluded_instances<Scalar, Index>(scene, options, progress);
        },
        "scene"_a,
        nb::kw_only(),
        "num_rays"_a = instance_defaults.num_rays,
        "batch_size"_a = instance_defaults.batch_size,
        "until_converged"_a = instance_defaults.until_converged,
        "is_occluder"_a = nb::none(),
        R"(Remove fully-occluded mesh instances from a scene.

:param scene:           Input scene.
:param num_rays:        Total ray budget. Must be > 0 unless ``until_converged`` is True.
:param batch_size:      Rays per batch.
:param until_converged: Stop early when a batch finds no new visible instances.
:param is_occluder:     Optional callable ``(mesh_index, instance_index) -> bool`` returning
                        whether an instance should block rays. Non-occluders are still tested
                        for visibility but do not contribute to the ray-caster scene. Defaults
                        to None (every instance is an occluder).

:return: Scene with occluded instances removed.)");

    m.def(
        "estimate_occluded_instances",
        [](const SimpleScene3D& scene,
           uint64_t num_rays,
           uint64_t batch_size,
           bool until_converged,
           std::optional<IsOccluderFn> is_occluder) {
            raycasting::OccludedInstanceEstimateOptions options;
            options.num_rays = num_rays;
            options.batch_size = batch_size;
            options.until_converged = until_converged;
            ProgressCallback progress;
            std::vector<std::pair<Index, Index>> occluded;
            auto callback = [&](Index mi, Index ii) { occluded.emplace_back(mi, ii); };
            if (is_occluder) {
                raycasting::estimate_occluded_instances<Scalar, Index>(
                    scene,
                    callback,
                    options,
                    progress,
                    *is_occluder);
            } else {
                raycasting::estimate_occluded_instances<Scalar, Index>(
                    scene,
                    callback,
                    options,
                    progress);
            }
            return occluded;
        },
        "scene"_a,
        nb::kw_only(),
        "num_rays"_a = instance_defaults.num_rays,
        "batch_size"_a = instance_defaults.batch_size,
        "until_converged"_a = instance_defaults.until_converged,
        "is_occluder"_a = nb::none(),
        R"(Find mesh instances that are fully occluded by other geometry.

:param scene:           Input scene.
:param num_rays:        Total ray budget. Must be > 0 unless ``until_converged`` is True.
:param batch_size:      Rays per batch.
:param until_converged: Stop early when a batch finds no new visible instances.
:param is_occluder:     Optional callable ``(mesh_index, instance_index) -> bool``. See
                        :py:func:`remove_occluded_instances`.

:return: List of ``(mesh_index, instance_index)`` pairs for occluded instances.)");
}

} // namespace lagrange::python

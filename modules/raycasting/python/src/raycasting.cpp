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

#include <lagrange/python/binding.h>

#include <lagrange/python/raycasting.h>

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
#include <lagrange/views.h>

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
using IsOccluderFn = std::function<bool(Index, Index)>;
// Python None | float scalar | float numpy array
using FloatArray = nb::ndarray<float, nb::numpy, nb::c_contig, nb::device::cpu>;
using FloatParam = std::variant<std::monostate, float, FloatArray>;
// Python None | int (AttributeId) | 3D vector
using DirectionParam = std::variant<std::monostate, AttributeId, Eigen::Vector3f>;

using NDArray3D = nb::ndarray<Scalar, nb::numpy, nb::shape<-1, 3>, nb::c_contig, nb::device::cpu>;

IsOccluderFn resolve_is_occluder(const std::optional<IsOccluderFn>& is_occluder)
{
    return is_occluder.value_or([](Index, Index) { return true; });
}

raycasting::RemoveOccludedInstancesOptions make_occluded_instance_options(
    uint64_t num_rays,
    uint64_t batch_size,
    bool until_converged,
    double threshold,
    double size_influence,
    double confidence)
{
    raycasting::RemoveOccludedInstancesOptions options;
    options.estimate_options = {num_rays, batch_size, until_converged};
    options.sampler_options = {threshold, size_influence, confidence};
    return options;
}

raycasting::RemoveOccludedFacetsOptions make_occluded_facet_options(
    uint64_t num_rays,
    uint64_t batch_size,
    bool adaptive,
    size_t num_adaptive_per_cosine,
    double vmf_kappa,
    double max_visibility_threshold,
    double size_influence,
    double confidence,
    raycasting::InstancingPolicy instancing)
{
    raycasting::RemoveOccludedFacetsOptions options;
    options.sampler_options.num_rays = num_rays;
    options.sampler_options.batch_size = batch_size;
    if (adaptive) {
        options.sampler_options.adaptive =
            raycasting::AdaptiveOptions{num_adaptive_per_cosine, vmf_kappa};
    } else {
        options.sampler_options.adaptive.reset();
    }
    options.sampler_options.max_visibility_threshold = max_visibility_threshold;
    options.sampler_options.size_influence = size_influence;
    options.sampler_options.confidence = confidence;
    options.instancing = instancing;
    return options;
}

raycasting::OccludedFacetViewErrorOptions make_view_error_options(
    double max_view_error,
    std::optional<std::vector<double>> graphcut_smoothness_candidates,
    uint64_t num_threshold_iterations,
    uint64_t num_refinement_iterations,
    uint64_t num_views,
    std::optional<double> smoothness_tradeoff)
{
    raycasting::OccludedFacetViewErrorOptions options;
    options.max_view_error = max_view_error;
    if (graphcut_smoothness_candidates)
        options.graphcut_smoothness_candidates = std::move(*graphcut_smoothness_candidates);
    options.num_threshold_iterations = num_threshold_iterations;
    options.num_refinement_iterations = num_refinement_iterations;
    options.num_views = num_views;
    options.smoothness_tradeoff = smoothness_tradeoff;
    return options;
}

auto make_view_error_evaluator(nb::callable evaluate)
{
    return
        [callback = std::move(
             evaluate)](span<const uint8_t> keep, span<const uint64_t> views, span<double> errors) {
            const auto values = nb::cast<std::vector<double>>(
                callback(span_to_tensor(keep, nb::handle()), span_to_tensor(views, nb::handle())));
            la_runtime_assert(
                values.size() == errors.size(),
                "evaluator returned incorrect number of errors");
            std::copy(values.begin(), values.end(), errors.begin());
        };
}

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

    nb::enum_<raycasting::InstancingPolicy>(
        m,
        "InstancingPolicy",
        "How a facet's measure is reconciled across the instances of its source mesh.")
        .value(
            "FlattenInstances",
            raycasting::InstancingPolicy::FlattenInstances,
            "One output mesh per instance; instancing is lost.")
        .value(
            "Max",
            raycasting::InstancingPolicy::Max,
            "Preserve instancing; measure is the max (most visible) over its instances.")
        .value(
            "Average",
            raycasting::InstancingPolicy::Average,
            "Preserve instancing; measure is the mean over its instances.");

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
    constexpr raycasting::OccludedFacetSamplerOptions facet_sampler_defaults{};
    constexpr raycasting::RemoveOccludedFacetsOptions facet_options_defaults{};
    constexpr raycasting::AdaptiveOptions adaptive_defaults{};
    constexpr raycasting::OccludedInstanceEstimateOptions instance_defaults{};
    constexpr raycasting::OccludedInstanceSamplerOptions instance_sampler_defaults{};

    const raycasting::OccludedFacetViewErrorOptions view_error_defaults{};
    nb::class_<raycasting::OccludedFacetViewErrorResult>(
        m,
        "OccludedFacetViewErrorResult",
        "Chosen view-error filtering parameters, output statistics, and budget feasibility.")
        .def_ro(
            "graphcut_smoothness",
            &raycasting::OccludedFacetViewErrorResult::graphcut_smoothness,
            "Chosen graph-cut weight; 0 for threshold-only filtering or an all-kept fallback.")
        .def_ro(
            "threshold",
            &raycasting::OccludedFacetViewErrorResult::threshold,
            "Chosen score cutoff, not constrained by the sampling retirement cutoff.")
        .def_ro(
            "removed_fraction",
            &raycasting::OccludedFacetViewErrorResult::removed_fraction,
            "Fraction of input instance-facets removed, not weighted by area.")
        .def_ro(
            "num_isolated_facets",
            &raycasting::OccludedFacetViewErrorResult::num_isolated_facets,
            "Kept facets with no kept edge-neighbor, not the number of disconnected components.")
        .def_ro(
            "worst_view_error",
            &raycasting::OccludedFacetViewErrorResult::worst_view_error,
            "Achieved worst error across all views for the returned mask, including failed "
            "baselines.")
        .def_ro(
            "within_error_budget",
            &raycasting::OccludedFacetViewErrorResult::within_error_budget,
            "Whether the returned mask satisfies the error budget; not an optimality guarantee.");

    using FacetsFilter = raycasting::OccludedFacetsFilter<Scalar, Index>;
    nb::class_<FacetsFilter>(
        m,
        "OccludedFacetsFilter",
        R"(Estimate per-facet visibility by sampling rays, then cache the results for later filtering.

The source scene is borrowed and kept alive by this object. Keep the scene and its backing arrays
unchanged while the filter is in use. Estimates are stored in double precision for each instance.
This class is move-only in C++. Adjacency is cached lazily for graph cuts and view-error statistics;
every cut builds a fresh graph, with no residual graph retained. External attribute policies must
permit copying or creating internal buffers as needed.)")
        .def(
            "__init__",
            [](FacetsFilter* self,
               const SimpleScene3D& scene,
               uint64_t num_rays,
               uint64_t batch_size,
               bool adaptive,
               size_t num_adaptive_per_cosine,
               double vmf_kappa,
               double max_visibility_threshold,
               double size_influence,
               double confidence,
               std::optional<IsOccluderFn> is_occluder) {
                const auto options = make_occluded_facet_options(
                    num_rays,
                    batch_size,
                    adaptive,
                    num_adaptive_per_cosine,
                    vmf_kappa,
                    max_visibility_threshold,
                    size_influence,
                    confidence,
                    raycasting::InstancingPolicy::Max);
                ProgressCallback progress;
                const auto occluder = resolve_is_occluder(is_occluder);
                new (self) FacetsFilter(scene, options.sampler_options, progress, occluder);
            },
            "scene"_a,
            nb::keep_alive<1, 2>(),
            nb::kw_only(),
            "num_rays"_a = facet_sampler_defaults.num_rays,
            "batch_size"_a = facet_sampler_defaults.batch_size,
            "adaptive"_a = facet_sampler_defaults.adaptive.has_value(),
            "num_adaptive_per_cosine"_a = adaptive_defaults.num_adaptive_per_cosine,
            "vmf_kappa"_a = adaptive_defaults.vmf_kappa,
            "max_visibility_threshold"_a = facet_sampler_defaults.max_visibility_threshold,
            "size_influence"_a = facet_sampler_defaults.size_influence,
            "confidence"_a = facet_sampler_defaults.confidence,
            "is_occluder"_a = nb::none(),
            R"(Estimate per-facet visibility by sampling rays, then cache the results for later filtering.

The source scene is kept alive until this filter is released and must remain unchanged, including
its backing arrays. Sampling retirement uses ``max_visibility_threshold``. Subsequent calls to
``apply()`` require ``0 <= visibility_threshold <= max_visibility_threshold``; this bounds the
filtering cutoff, not the facet measures. The measurement and filtering methods never resample
or invoke ``is_occluder`` again.
``apply_with_view_error()`` may choose a cutoff above the retirement cutoff; it uses the cached
measures as a heuristic and independently validates candidate masks against the view-error budget.

:param scene:                    Source triangle-mesh scene, retained without copying.
:param num_rays:                 Total sampling ray budget. Must be > 0.
:param batch_size:               Rays per batch.
:param adaptive:                 Enable adaptive sampling; False uses plain cosine sampling.
:param num_adaptive_per_cosine:   Seed-guided rays per cosine ray (>= 1), used in adaptive mode.
:param vmf_kappa:                Positive adaptive vMF concentration.
:param max_visibility_threshold: Retirement cutoff in [0, 0.5], bounding ordinary ``apply()``'s
                                 visibility_threshold parameter.
:param size_influence:           Non-negative exponent on facet area relative to mean face area.
:param confidence:              Per-facet early-keep confidence in (0, 1).
:param is_occluder:              Optional predicate selecting which instances block rays.)")
        .def_prop_ro(
            "max_visibility_threshold",
            &FacetsFilter::get_max_visibility_threshold,
            "Sampling retirement cutoff and maximum ordinary apply() parameter, not a facet "
            "visibility "
            "bound.")
        .def(
            "estimate_visibility",
            &FacetsFilter::estimate_visibility,
            "attribute_name"_a,
            nb::kw_only(),
            "instancing"_a = facet_options_defaults.instancing,
            R"(Export facet visibility estimates as a per-facet float64 attribute.

Cached constructor measures are aggregated across instances according to ``instancing``.
The borrowed source is unchanged; no resampling or adjacency initialization is performed.
The single-channel output attribute has shape ``(num_facets,)``.

:param attribute_name: Name of the per-facet visibility attribute to create on each output mesh.
:param instancing:     Max/Average preserve sharing; FlattenInstances annotates each instance.
:return: A new scene carrying the facet visibility attribute.)")
        .def(
            "apply",
            [](const FacetsFilter& facet_filter,
               double visibility_threshold,
               std::optional<double> graphcut_smoothness,
               raycasting::InstancingPolicy instancing,
               std::optional<std::string> visibility_attribute_name,
               std::optional<std::string> keep_attribute_name) {
                raycasting::OccludedFacetFilterOptions options;
                options.visibility_threshold = visibility_threshold;
                options.graphcut_smoothness = graphcut_smoothness;
                options.instancing = instancing;
                options.visibility_attribute_name = std::move(visibility_attribute_name);
                options.keep_attribute_name = std::move(keep_attribute_name);
                return facet_filter.apply(options);
            },
            nb::kw_only(),
            "visibility_threshold"_a,
            "graphcut_smoothness"_a = nb::none(),
            "instancing"_a = facet_options_defaults.instancing,
            "visibility_attribute_name"_a = nb::none(),
            "keep_attribute_name"_a = nb::none(),
            R"(Apply the occluded-facet filter using cached constructor estimates without resampling.

Visibility measures are aggregated across instances according to ``instancing``. Without
smoothing, facets below ``visibility_threshold`` are removed. Positive smoothing applies a
fresh graph cut to the internal double-precision measures and may override independent labels.
Callers must set external attribute policies to permit copying or internalizing buffers as needed.

If ``visibility_attribute_name`` is specified, visibility measures are exported as a float64
facet attribute before any pruning. If ``keep_attribute_name`` is specified, no facets are removed;
instead, a uint8 facet mask (1 keep, 0 cull) is exported. Both attributes have shape
``(num_facets,)``, including ``(0,)`` on initially zero-facet meshes. Output names must be distinct
and must not already exist on the source meshes; existing attributes are never overwritten.

:param visibility_threshold: Required finite cutoff in [0, self.max_visibility_threshold].
:param graphcut_smoothness:   None disables graph cutting; 0 gives independent thresholding.
                             Positive finite values favor coherent keep/cull regions.
:param instancing:           Max/Average preserve sharing; FlattenInstances classifies each instance.
:param visibility_attribute_name: Optional name for the exported per-facet visibility attribute.
:param keep_attribute_name:  Optional name for the per-facet keep mask; when specified, no facet
                            is removed.
:return: A new scene, pruned unless keep_attribute_name is specified.
:raises RuntimeError: Invalid thresholds/weights, identical output names, or attribute creation
                      errors, including existing source attributes.)")
        .def(
            "apply_with_view_error",
            [](const FacetsFilter& facet_filter,
               nb::callable evaluate,
               double max_view_error,
               std::optional<std::vector<double>> graphcut_smoothness_candidates,
               uint64_t num_threshold_iterations,
               uint64_t num_refinement_iterations,
               uint64_t num_views,
               std::optional<double> smoothness_tradeoff) {
                const auto options = make_view_error_options(
                    max_view_error,
                    std::move(graphcut_smoothness_candidates),
                    num_threshold_iterations,
                    num_refinement_iterations,
                    num_views,
                    smoothness_tradeoff);
                auto evaluate_cpp = make_view_error_evaluator(std::move(evaluate));
                auto [result, parameters] =
                    facet_filter.apply_with_view_error(evaluate_cpp, options);
                return nb::make_tuple(std::move(result), parameters);
            },
            "evaluate"_a,
            nb::kw_only(),
            "max_view_error"_a = view_error_defaults.max_view_error,
            "graphcut_smoothness_candidates"_a = nb::none(),
            "num_threshold_iterations"_a = view_error_defaults.num_threshold_iterations,
            "num_refinement_iterations"_a = view_error_defaults.num_refinement_iterations,
            "num_views"_a,
            "smoothness_tradeoff"_a = view_error_defaults.smoothness_tradeoff,
            R"(Search graph-cut parameters using cached measures and a user-supplied view-error budget.

No rays are traced and the source scene remains unchanged. Instances are flattened, and the
returned mask is reconstructed on the same global graph used during probing. The score cutoff
may exceed max_visibility_threshold: estimates guide the search, while the evaluator validates
candidate masks. This is a heuristic search, not a global-optimality guarantee.

:param evaluate: Callable ``(keep_mask, view_indices) -> errors``. The uint8 mask is ordered by
                 source mesh, instance, then local facet. uint64 indices are empty for all views;
                 return one finite non-negative error per requested view. Calls are synchronous.
:param max_view_error: Finite non-negative worst-view error budget; 0 requires an exact match.
:param graphcut_smoothness_candidates: Finite positive weights, sorted and deduplicated internally;
                                      None or an empty list selects the automatic sweep.
:param num_threshold_iterations: Positive iterations per primary threshold search.
:param num_refinement_iterations: Positive iterations per verification refinement search.
:param num_views: Positive number of errors returned for an empty view-index list.
:param smoothness_tradeoff: Optional fraction of baseline removal that may be sacrificed for
                           smoother regions. None disables the sweep; 0 preserves removal count,
                           and 1 permits sacrificing all removal. Present values must be in [0, 1].
:return: Tuple ``(scene, parameters)`` with an OccludedFacetViewErrorResult. A failed all-kept
         baseline retains the original facets and reports within_error_budget=False.
:raises RuntimeError: Invalid options, invalid evaluator errors, or incorrect error counts.)");

    m.def(
        "remove_occluded_facets",
        [](const SimpleScene3D& scene,
           uint64_t num_rays,
           uint64_t batch_size,
           bool adaptive,
           size_t num_adaptive_per_cosine,
           double vmf_kappa,
           double threshold,
           double size_influence,
           double confidence,
           raycasting::InstancingPolicy instancing,
           std::optional<double> graphcut_smoothness,
           std::optional<IsOccluderFn> is_occluder) {
            auto options = make_occluded_facet_options(
                num_rays,
                batch_size,
                adaptive,
                num_adaptive_per_cosine,
                vmf_kappa,
                threshold,
                size_influence,
                confidence,
                instancing);
            options.graphcut_smoothness = graphcut_smoothness;
            ProgressCallback progress;
            const auto occluder = resolve_is_occluder(is_occluder);
            return raycasting::remove_occluded_facets<Scalar, Index>(
                scene,
                options,
                progress,
                occluder);
        },
        "scene"_a,
        nb::kw_only(),
        "num_rays"_a = facet_sampler_defaults.num_rays,
        "batch_size"_a = facet_sampler_defaults.batch_size,
        "adaptive"_a = facet_sampler_defaults.adaptive.has_value(),
        "num_adaptive_per_cosine"_a = adaptive_defaults.num_adaptive_per_cosine,
        "vmf_kappa"_a = adaptive_defaults.vmf_kappa,
        "threshold"_a = facet_sampler_defaults.max_visibility_threshold,
        "size_influence"_a = facet_sampler_defaults.size_influence,
        "confidence"_a = facet_sampler_defaults.confidence,
        "instancing"_a = facet_options_defaults.instancing,
        "graphcut_smoothness"_a = nb::none(),
        "is_occluder"_a = nb::none(),
        R"(Build a new scene with facets not visible from the outside removed.

Without smoothing, a facet is kept iff its size-weighted visibility measure
``mean_visibility * (area_f / mean_face_area) ** size_influence`` is at least ``threshold``, where
``mean_visibility`` is the cosine-weighted escaped fraction. Positive smoothing may override
these independent labels. The legacy ``threshold`` keyword sets both the sampler's
``max_visibility_threshold`` and the filter's ``visibility_threshold``, not a facet visibility bound.

:param scene:                   Input scene.
:param num_rays:                Total ray budget. Must be > 0.
:param batch_size:              Rays per batch.
:param adaptive:                Enable adaptive sampling (seed reuse to find small holes faster).
                                True by default; False uses plain cosine sampling.
:param num_adaptive_per_cosine: Seed-guided rays per cosine ray in the adaptive mixture (>= 1).
                                Ignored when ``adaptive`` is False.
:param vmf_kappa:               Concentration of the adaptive vMF proposal lobe (> 0; angular
                                width ~ 1/sqrt(kappa)). Ignored when ``adaptive`` is False.
:param threshold:               Keep-threshold in [0, 0.5] on the per-face visibility measure.
:param size_influence:          Exponent on area_f / mean_face_area. 0 = keep any exposed facet
                                (pure occlusion culling); > 0 also culls facets small relative to
                                their mesh (visibility-weighted decimation).
:param confidence:              Per-facet confidence in (0, 1) for anytime-valid early keeping.
                                Higher values retire later. It does not control final removals or
                                joint confidence across the scene.
:param instancing:              How to reconcile a facet's measure across instances of its source
                                mesh: ``Max`` (default, most visible) or ``Average`` (both preserve
                                instancing), or ``FlattenInstances`` (one mesh per instance).
:param graphcut_smoothness:      Optional graph-cut smoothing penalty on the per-mesh decision.
                                None disables graph cutting; 0 gives independent thresholding,
                                while positive values favor coherent regions.
:param is_occluder:             Optional callable ``(mesh_index, instance_index) -> bool``
                                returning whether an instance should block rays. Non-occluders
                                are still tested for visibility but do not contribute to the
                                ray-caster scene. Defaults to None (every instance is an
                                occluder).
:return: Scene with occluded facets removed.)");

    m.def(
        "remove_occluded_facets_with_view_error",
        [](const SimpleScene3D& scene,
           nb::callable evaluate,
           double max_view_error,
           std::optional<std::vector<double>> graphcut_smoothness_candidates,
           uint64_t num_threshold_iterations,
           uint64_t num_refinement_iterations,
           uint64_t num_views,
           std::optional<double> smoothness_tradeoff,
           uint64_t num_rays,
           uint64_t batch_size,
           bool adaptive,
           size_t num_adaptive_per_cosine,
           double vmf_kappa,
           double threshold,
           double size_influence,
           double confidence,
           std::optional<IsOccluderFn> is_occluder) {
            const auto options = make_occluded_facet_options(
                num_rays,
                batch_size,
                adaptive,
                num_adaptive_per_cosine,
                vmf_kappa,
                threshold,
                size_influence,
                confidence,
                raycasting::InstancingPolicy::FlattenInstances);
            const auto view_options = make_view_error_options(
                max_view_error,
                std::move(graphcut_smoothness_candidates),
                num_threshold_iterations,
                num_refinement_iterations,
                num_views,
                smoothness_tradeoff);
            auto evaluate_cpp = make_view_error_evaluator(std::move(evaluate));
            ProgressCallback progress;
            const auto occluder = resolve_is_occluder(is_occluder);
            auto [result, parameters] =
                raycasting::remove_occluded_facets_with_view_error<Scalar, Index>(
                    scene,
                    evaluate_cpp,
                    options,
                    view_options,
                    progress,
                    occluder);
            return nb::make_tuple(std::move(result), parameters);
        },
        "scene"_a,
        "evaluate"_a,
        nb::kw_only(),
        "max_view_error"_a = view_error_defaults.max_view_error,
        "graphcut_smoothness_candidates"_a = nb::none(),
        "num_threshold_iterations"_a = view_error_defaults.num_threshold_iterations,
        "num_refinement_iterations"_a = view_error_defaults.num_refinement_iterations,
        "num_views"_a,
        "smoothness_tradeoff"_a = view_error_defaults.smoothness_tradeoff,
        "num_rays"_a = facet_sampler_defaults.num_rays,
        "batch_size"_a = facet_sampler_defaults.batch_size,
        "adaptive"_a = false,
        "num_adaptive_per_cosine"_a = adaptive_defaults.num_adaptive_per_cosine,
        "vmf_kappa"_a = adaptive_defaults.vmf_kappa,
        "threshold"_a = facet_sampler_defaults.max_visibility_threshold,
        "size_influence"_a = facet_sampler_defaults.size_influence,
        "confidence"_a = facet_sampler_defaults.confidence,
        "is_occluder"_a = nb::none(),
        R"(Sample visibility once and filter facets using a user-supplied view-error budget.

This convenience wrapper constructs an OccludedFacetsFilter and calls apply_with_view_error().
The result is flattened, with one output mesh per surviving instance. Cached visibility scores
only guide the heuristic search; the evaluator defines the error metric and validates the masks.

:param scene: Input triangle-mesh scene.
:param evaluate: Callable ``(keep_mask, view_indices) -> errors``. The uint8 keep mask is ordered
                 by source mesh, instance, then local facet. uint64 view indices are empty for all
                 views; return one finite non-negative error per requested view.
:param max_view_error: Finite non-negative worst-view error budget; 0 requires an exact match.
:param graphcut_smoothness_candidates: Finite positive weights, sorted and deduplicated internally;
                                      None or an empty list selects the automatic sweep.
:param num_threshold_iterations: Positive iterations per primary threshold search.
:param num_refinement_iterations: Positive iterations per verification refinement search.
:param num_views: Positive number of errors returned when the view-index list is empty.
:param smoothness_tradeoff: Optional fraction of baseline removal that may be sacrificed for
                           smoother regions. None disables the sweep; 0 preserves removal count,
                           and 1 permits sacrificing all removal. Present values must be in [0, 1].
:param num_rays: Total sampling ray budget.
:param batch_size: Rays per batch.
:param adaptive: Enable adaptive sampling; False uses plain cosine sampling (default).
:param num_adaptive_per_cosine: Seed-guided rays per cosine ray when adaptive.
:param vmf_kappa: Adaptive vMF lobe concentration.
:param threshold: Sampler max_visibility_threshold retirement cutoff, not a bound on facet scores
                  or the cutoff selected by view-error filtering.
:param size_influence: Exponent on facet area relative to mean facet area.
:param confidence: Per-facet confidence for anytime-valid early keeping.
:param is_occluder: Optional callable ``(mesh_index, instance_index) -> bool``.
:return: Tuple ``(scene, parameters)`` with an OccludedFacetViewErrorResult.
:raises RuntimeError: Invalid options, invalid evaluator errors, or incorrect error counts.)");

    m.def(
        "remove_occluded_instances",
        [](const SimpleScene3D& scene,
           uint64_t num_rays,
           uint64_t batch_size,
           bool until_converged,
           double threshold,
           double size_influence,
           double confidence,
           std::optional<IsOccluderFn> is_occluder) {
            const auto options = make_occluded_instance_options(
                num_rays,
                batch_size,
                until_converged,
                threshold,
                size_influence,
                confidence);
            ProgressCallback progress;
            // Explicit template args bypass deduction — function_ref is constructed from
            // std::function via the implicit conversion only after deduction is settled.
            const auto occluder = resolve_is_occluder(is_occluder);
            return raycasting::remove_occluded_instances<Scalar, Index>(
                scene,
                options,
                progress,
                occluder);
        },
        "scene"_a,
        nb::kw_only(),
        "num_rays"_a = instance_defaults.num_rays,
        "batch_size"_a = instance_defaults.batch_size,
        "until_converged"_a = instance_defaults.until_converged,
        "threshold"_a = instance_sampler_defaults.threshold,
        "size_influence"_a = instance_sampler_defaults.size_influence,
        "confidence"_a = instance_sampler_defaults.confidence,
        "is_occluder"_a = nb::none(),
        R"(Remove instances whose exterior visibility measure is below ``threshold``.

Keeps an instance when its visibility measure—mean visibility weighted by relative surface
area—is at least ``threshold``. Unlike a per-point test, this can grow with mesh size: a large mesh
seen through a small hole survives, while a small or barely visible one is culled.

:param scene:           Input scene.
:param num_rays:        Total ray budget. Must be > 0 unless ``until_converged`` is True.
:param batch_size:      Rays per batch.
:param until_converged: After the initial batch, stop when a batch confidently retires no new
                        instances. This is a batch-size-dependent heuristic.
:param threshold:       Keep-threshold in [0, 0.5]: keep when mean_visibility * (area/scene-AABB-area)
                        ** size_influence >= threshold. Defaults to ``1e-6`` (matching the C++ API);
                        larger values enable aggressive culling, and a near-zero value reproduces the
                        original first-observed-escape behavior.
:param size_influence:  How much mesh size counts (exponent on area/scene-AABB-area). 0 = size-
                        independent (keeps small exterior details like emblems); 1 = area-weighted
                        visibility; 0.5 = weighted by linear size.
:param confidence:      Per-instance confidence in (0, 1) used at each batch-end early-keep test.
                        Higher values retire later; it does not control final removals or joint
                        confidence across the scene.
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
           double threshold,
           double size_influence,
           double confidence,
           std::optional<IsOccluderFn> is_occluder) {
            const auto options = make_occluded_instance_options(
                num_rays,
                batch_size,
                until_converged,
                threshold,
                size_influence,
                confidence);
            ProgressCallback progress;
            const auto occluder = resolve_is_occluder(is_occluder);
            const auto measures = raycasting::estimate_occluded_instance_measures<Scalar, Index>(
                scene,
                options,
                progress,
                occluder);
            std::vector<std::pair<Index, Index>> occluded;
            for (Index mi = 0; mi < scene.get_num_meshes(); ++mi) {
                for (Index ii = 0; ii < scene.get_num_instances(mi); ++ii) {
                    if (measures[mi][ii] < threshold) occluded.emplace_back(mi, ii);
                }
            }
            return occluded;
        },
        "scene"_a,
        nb::kw_only(),
        "num_rays"_a = instance_defaults.num_rays,
        "batch_size"_a = instance_defaults.batch_size,
        "until_converged"_a = instance_defaults.until_converged,
        "threshold"_a = instance_sampler_defaults.threshold,
        "size_influence"_a = instance_sampler_defaults.size_influence,
        "confidence"_a = instance_sampler_defaults.confidence,
        "is_occluder"_a = nb::none(),
        R"(Find mesh instances whose visibility measure falls below the threshold.

Deprecated: use :py:func:`estimate_occluded_instance_measures` and apply the desired threshold to
the returned measures.

:param scene:           Input scene.
:param num_rays:        Total ray budget. Must be > 0 unless ``until_converged`` is True.
:param batch_size:      Rays per batch.
:param until_converged: After the initial batch, stop when a batch confidently retires no new
                        instances. This is a batch-size-dependent heuristic.
:param threshold:       Keep-threshold in [0, 0.5]. Defaults to ``1e-6`` (matching the C++ API); a
                        near-zero value reproduces the original first-observed-escape behavior. See
                        :py:func:`remove_occluded_instances`.
:param size_influence:  Exponent on area/scene-AABB-area (0 = size-independent, 1 = area-weighted).
                        See :py:func:`remove_occluded_instances`.
:param confidence:      Per-instance confidence in (0, 1) used at each batch-end early-keep test.
                        Higher values retire later; it does not control final removals or joint
                        confidence across the scene.
:param is_occluder:     Optional callable ``(mesh_index, instance_index) -> bool``.

:return: List of ``(mesh_index, instance_index)`` pairs for occluded instances.)");

    m.def(
        "estimate_occluded_instance_measures",
        [](const SimpleScene3D& scene,
           uint64_t num_rays,
           uint64_t batch_size,
           bool until_converged,
           double threshold,
           double size_influence,
           double confidence,
           std::optional<IsOccluderFn> is_occluder) {
            const auto options = make_occluded_instance_options(
                num_rays,
                batch_size,
                until_converged,
                threshold,
                size_influence,
                confidence);
            ProgressCallback progress;
            const auto occluder = resolve_is_occluder(is_occluder);
            return raycasting::estimate_occluded_instance_measures<Scalar, Index>(
                scene,
                options,
                progress,
                occluder);
        },
        "scene"_a,
        nb::kw_only(),
        "num_rays"_a = instance_defaults.num_rays,
        "batch_size"_a = instance_defaults.batch_size,
        "until_converged"_a = instance_defaults.until_converged,
        "threshold"_a = instance_sampler_defaults.threshold,
        "size_influence"_a = instance_sampler_defaults.size_influence,
        "confidence"_a = instance_sampler_defaults.confidence,
        "is_occluder"_a = nb::none(),
        R"(Estimate every instance's visibility measure, indexed ``[mesh_index][instance_index]``.

The sampler runs at ``threshold`` (the strictest threshold you intend to use); the returned
measures let you keep instances at any ``t <= threshold`` without re-tracing — keep
``(mesh_index, instance_index)`` iff ``measures[mesh_index][instance_index] >= t``.

All instance functions default ``threshold`` to ``1e-6`` (matching the C++ API); pass a near-zero
value to reproduce the original first-observed-escape behavior.

:param scene:           Input scene.
:param num_rays:        Total ray budget. Must be > 0 unless ``until_converged`` is True.
:param batch_size:      Rays per batch.
:param until_converged: After the initial batch, stop when a batch confidently retires no new
                        instances. This is a batch-size-dependent heuristic.
:param threshold:       Strictest keep-threshold in [0, 0.5] to sample for. See
                        :py:func:`remove_occluded_instances`.
:param size_influence:  Exponent on area/scene-AABB-area (0 = size-independent, 1 = area-weighted).
:param confidence:      Per-instance confidence in (0, 1) used at each batch-end early-keep test.
                        Higher values retire later; it does not control final removals or joint
                        confidence across the scene.
:param is_occluder:     Optional callable ``(mesh_index, instance_index) -> bool``.

:return: Per-instance measures as ``measures[mesh_index][instance_index]``.)");

    m.def(
        "estimate_facet_visibility",
        [](const SimpleScene3D& scene,
           std::string attribute_name,
           uint64_t num_rays,
           uint64_t batch_size,
           bool adaptive,
           size_t num_adaptive_per_cosine,
           double vmf_kappa,
           double threshold,
           double size_influence,
           double confidence,
           raycasting::InstancingPolicy instancing,
           std::optional<IsOccluderFn> is_occluder) {
            const auto options = make_occluded_facet_options(
                num_rays,
                batch_size,
                adaptive,
                num_adaptive_per_cosine,
                vmf_kappa,
                threshold,
                size_influence,
                confidence,
                instancing);
            ProgressCallback progress;
            const auto occluder = resolve_is_occluder(is_occluder);
            return raycasting::estimate_facet_visibility<Scalar, Index>(
                scene,
                attribute_name,
                options,
                progress,
                occluder);
        },
        "scene"_a,
        nb::kw_only(),
        "attribute_name"_a = "visibility_measure",
        "num_rays"_a = facet_sampler_defaults.num_rays,
        "batch_size"_a = facet_sampler_defaults.batch_size,
        "adaptive"_a = facet_sampler_defaults.adaptive.has_value(),
        "num_adaptive_per_cosine"_a = adaptive_defaults.num_adaptive_per_cosine,
        "vmf_kappa"_a = adaptive_defaults.vmf_kappa,
        "threshold"_a = facet_sampler_defaults.max_visibility_threshold,
        "size_influence"_a = facet_sampler_defaults.size_influence,
        "confidence"_a = facet_sampler_defaults.confidence,
        "instancing"_a = facet_options_defaults.instancing,
        "is_occluder"_a = nb::none(),
        R"(Estimate each facet's visibility measure and write it to a named per-facet attribute.

Each output mesh carries a per-facet Scalar attribute ``attribute_name`` equal to the facet's
(possibly aggregated) visibility measure
``mean_visibility * (area_f / mean_face_area) ** size_influence``. The legacy ``threshold`` keyword
sets the sampler's ``max_visibility_threshold`` retirement cutoff; remove facets whose attribute
is below any ``0 <= visibility_threshold <= threshold`` to filter without re-tracing.
The maximum bounds the filtering cutoff, not the returned facet measures.

:param scene:                   Input scene.
:param attribute_name:          Name of the per-facet Scalar attribute to create on each mesh.
:param num_rays:                Total ray budget. Must be > 0.
:param batch_size:              Rays per batch.
:param adaptive:                Enable adaptive sampling (seed reuse to find small holes faster).
                                True by default; False uses plain cosine sampling.
:param num_adaptive_per_cosine: Seed-guided rays per cosine ray (>= 1). Ignored when not adaptive.
:param vmf_kappa:               Concentration of the adaptive vMF proposal lobe (> 0). Ignored
                                when not adaptive.
:param threshold:               Strictest keep-threshold in [0, 0.5] to sample for.
:param size_influence:          Exponent on area_f / mean_face_area (0 = pure occlusion culling).
:param confidence:              Per-facet confidence in (0, 1) for anytime-valid early keeping.
                                Higher values retire later. It does not control final removals or
                                joint confidence across the scene.
:param instancing:              How to reconcile a facet's measure across instances of its source
                                mesh: ``Max`` (default, most visible) or ``Average`` (both preserve
                                instancing), or ``FlattenInstances`` (one mesh per instance).
:param is_occluder:             Optional callable ``(mesh_index, instance_index) -> bool``.

:return: Scene whose meshes carry the per-facet visibility measure attribute, per ``instancing``.)");
}

} // namespace lagrange::python

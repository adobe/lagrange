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
#include <lagrange/utils/BitField.h>

#include <vector>

namespace nb = nanobind;
using namespace nb::literals;

namespace lagrange::python {

namespace {

using Scalar = double;
using Index = uint32_t;
using MeshType = SurfaceMesh<Scalar, Index>;

using NDArray3D = nb::ndarray<Scalar, nb::numpy, nb::shape<-1, 3>, nb::c_contig, nb::device::cpu>;

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
:param visible:        True to make visible, False to hide.)");

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
           nb::object direction,
           raycasting::CastMode cast_mode,
           raycasting::FallbackMode fallback_mode,
           double default_value,
           const raycasting::RayCaster* ray_caster) {
            raycasting::ProjectOptions opts;
            opts.attribute_ids = std::move(attribute_ids);
            opts.project_vertices = project_vertices;
            opts.project_mode = project_mode;
            if (direction.is_none()) {
                opts.direction = std::monostate{};
            } else if (nb::isinstance<nb::int_>(direction)) {
                opts.direction = nb::cast<AttributeId>(direction);
            } else {
                opts.direction = nb::cast<Eigen::Vector3f>(direction);
            }
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
           nb::object direction,
           raycasting::CastMode cast_mode,
           raycasting::FallbackMode fallback_mode,
           double default_value,
           const raycasting::RayCaster* ray_caster) {
            raycasting::ProjectDirectionalOptions opts;
            opts.attribute_ids = std::move(attribute_ids);
            opts.project_vertices = project_vertices;
            if (direction.is_none()) {
                opts.direction = std::monostate{};
            } else if (nb::isinstance<nb::int_>(direction)) {
                opts.direction = nb::cast<AttributeId>(direction);
            } else {
                opts.direction = nb::cast<Eigen::Vector3f>(direction);
            }
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
    // Instead of a SurfaceMesh target, accept an (N,3) NumPy array of query points (and optionally
    // directions). A temporary SurfaceMesh is created by wrapping the input buffer via
    // wrap_as_vertices (zero-copy). The input buffer is modified in place and returned by value.
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
}

} // namespace lagrange::python

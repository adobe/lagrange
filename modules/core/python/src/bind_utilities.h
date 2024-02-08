/*
 * Copyright 2022 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#pragma once

#include <lagrange/AttributeTypes.h>
#include <lagrange/NormalWeightingType.h>
#include <lagrange/combine_meshes.h>
#include <lagrange/compute_area.h>
#include <lagrange/compute_centroid.h>
#include <lagrange/compute_components.h>
#include <lagrange/compute_dihedral_angles.h>
#include <lagrange/compute_dijkstra_distance.h>
#include <lagrange/compute_edge_lengths.h>
#include <lagrange/compute_facet_normal.h>
#include <lagrange/compute_normal.h>
#include <lagrange/compute_tangent_bitangent.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/compute_vertex_valence.h>
#include <lagrange/extract_submesh.h>
#include <lagrange/map_attribute.h>
#include <lagrange/normalize_meshes.h>
#include <lagrange/permute_facets.h>
#include <lagrange/permute_vertices.h>
#include <lagrange/python/tensor_utils.h>
#include <lagrange/python/utils/StackVector.h>
#include <lagrange/remap_vertices.h>
#include <lagrange/separate_by_components.h>
#include <lagrange/separate_by_facet_groups.h>
#include <lagrange/topology.h>
#include <lagrange/transform_mesh.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/unify_index_buffer.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/weld_indexed_attribute.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanobind/nanobind.h>
#include <nanobind/eigen/dense.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/vector.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::python {

template <typename Scalar, typename Index>
void bind_utilities(nanobind::module_& m)
{
    namespace nb = nanobind;
    using namespace nb::literals;
    using MeshType = SurfaceMesh<Scalar, Index>;

    nb::enum_<NormalWeightingType>(m, "NormalWeightingType")
        .value("Uniform", NormalWeightingType::Uniform)
        .value("CornerTriangleArea", NormalWeightingType::CornerTriangleArea)
        .value("Angle", NormalWeightingType::Angle);

    nb::class_<VertexNormalOptions>(m, "VertexNormalOptions")
        .def(nb::init<>())
        .def_rw("output_attribute_name", &VertexNormalOptions::output_attribute_name)
        .def_rw("weight_type", &VertexNormalOptions::weight_type)
        .def_rw(
            "weighted_corner_normal_attribute_name",
            &VertexNormalOptions::weighted_corner_normal_attribute_name)
        .def_rw(
            "recompute_weighted_corner_normals",
            &VertexNormalOptions::recompute_weighted_corner_normals)
        .def_rw("keep_weighted_corner_normals", &VertexNormalOptions::keep_weighted_corner_normals);

    m.def(
        "compute_vertex_normal",
        &compute_vertex_normal<Scalar, Index>,
        "mesh"_a,
        "options"_a = VertexNormalOptions());

    nb::class_<FacetNormalOptions>(m, "FacetNormalOptions")
        .def(nb::init<>())
        .def_rw("output_attribute_name", &FacetNormalOptions::output_attribute_name);

    m.def(
        "compute_facet_normal",
        &compute_facet_normal<Scalar, Index>,
        "mesh"_a,
        "options"_a = FacetNormalOptions());

    nb::class_<NormalOptions>(m, "NormalOptions")
        .def(nb::init<>())
        .def_rw("output_attribute_name", &NormalOptions::output_attribute_name)
        .def_rw("weight_type", &NormalOptions::weight_type)
        .def_rw("facet_normal_attribute_name", &NormalOptions::facet_normal_attribute_name)
        .def_rw("recompute_facet_normals", &NormalOptions::recompute_facet_normals)
        .def_rw("keep_facet_normals", &NormalOptions::keep_facet_normals);

    m.def(
        "compute_normal",
        [](MeshType& mesh,
           Scalar feature_angle_threshold,
           nb::object cone_vertices,
           std::optional<NormalOptions> normal_options) {
            NormalOptions options;
            if (normal_options.has_value()) {
                options = std::move(normal_options.value());
            }

            if (cone_vertices.is_none()) {
                return compute_normal<Scalar, Index>(mesh, feature_angle_threshold, {}, options);
            } else if (nb::isinstance<nb::list>(cone_vertices)) {
                auto cone_vertices_list = nb::cast<std::vector<Index>>(cone_vertices);
                span<const Index> data{cone_vertices_list.data(), cone_vertices_list.size()};
                return compute_normal<Scalar, Index>(mesh, feature_angle_threshold, data, options);
            } else if (nb::isinstance<Tensor<Index>>(cone_vertices)) {
                auto cone_vertices_tensor = nb::cast<Tensor<Index>>(cone_vertices);
                auto [data, shape, stride] = tensor_to_span(cone_vertices_tensor);
                la_runtime_assert(is_dense(shape, stride));
                return compute_normal<Scalar, Index>(mesh, feature_angle_threshold, data, options);
            } else {
                throw std::runtime_error("Invalid cone_vertices type");
            }
        },
        "mesh"_a,
        "feature_angle_threshold"_a = M_PI / 4,
        "cone_vertices"_a = nb::none(),
        "options"_a = nb::none(),
        R"(Compute indexed normal attribute.

Edge with dihedral angles larger than `feature_angle_threshold` are considered as sharp edges.
Vertices listed in `cone_vertices` are considered as cone vertices, which is always sharp.

:param mesh: input mesh
:type mesh: SurfaceMesh
:param feature_angle_threshold: feature angle threshold
:type feature_angle_threshold: float, optional
:param cone_vertices: cone vertices
:type cone_vertices: list[int] or numpy.ndarray, optional
:param options: normal options
:type optionas: NormalOptions, optional

:returns: the id of the indexed normal attribute.
)");

    m.def("normalize_mesh", &normalize_mesh<Scalar, Index>);
    m.def("normalize_meshes", [](std::vector<SurfaceMesh<Scalar, Index>*> meshes) {
        span<SurfaceMesh<Scalar, Index>*> meshes_span(meshes.data(), meshes.size());
        normalize_meshes(meshes_span);
    });

    m.def(
        "combine_meshes",
        [](std::vector<SurfaceMesh<Scalar, Index>*> meshes, bool preserve_vertices) {
            return combine_meshes<Scalar, Index>(
                meshes.size(),
                [&](size_t i) -> const SurfaceMesh<Scalar, Index>& { return *meshes[i]; },
                preserve_vertices);
        },
        "meshes"_a,
        "preserve_attributes"_a = true);

    m.def(
        "unify_index_buffer",
        [](MeshType& mesh) { return unify_index_buffer(mesh); },
        "mesh"_a,
        R"(Unify the index buffer of the mesh.  All indexed attributes will be unified.

:param mesh: The mesh to unify.
:type mesh: SurfaceMesh

:returns: The unified mesh.)");

    m.def(
        "unify_index_buffer",
        &lagrange::unify_index_buffer<Scalar, Index>,
        "mesh"_a,
        "attribute_ids"_a,
        R"(Unify the index buffer of the mesh for selected attributes.

:param mesh: The mesh to unify.
:type mesh: SurfaceMesh
:param attribute_ids: The selected attribute ids to unify.
:type attribute_ids: list of int

:returns: The unified mesh.)");

    m.def(
        "unify_index_buffer",
        &lagrange::unify_named_index_buffer<Scalar, Index>,
        "mesh"_a,
        "attribute_names"_a,
        R"(Unify the index buffer of the mesh for selected attributes.

:param mesh: The mesh to unify.
:type mesh: SurfaceMesh
:param attribute_names: The selected attribute names to unify.
:type attribute_names: list of str

:returns: The unified mesh.)");

    m.def(
        "triangulate_polygonal_facets",
        &lagrange::triangulate_polygonal_facets<Scalar, Index>,
        "mesh"_a);

    nb::enum_<ComponentOptions::ConnectivityType>(m, "ConnectivityType")
        .value("Vertex", ComponentOptions::ConnectivityType::Vertex)
        .value("Edge", ComponentOptions::ConnectivityType::Edge);

    m.def(
        "compute_components",
        [](MeshType& mesh,
           std::optional<std::string_view> output_attribute_name,
           std::optional<lagrange::ConnectivityType> connectivity_type,
           std::optional<nb::list>& blocker_elements) {
            lagrange::ComponentOptions opt;
            if (output_attribute_name.has_value()) {
                opt.output_attribute_name = output_attribute_name.value();
            }
            if (connectivity_type.has_value()) {
                opt.connectivity_type = connectivity_type.value();
            }
            std::vector<Index> blocker_elements_vec;
            if (blocker_elements.has_value()) {
                for (auto val : blocker_elements.value()) {
                    blocker_elements_vec.push_back(nb::cast<Index>(val));
                }
            }
            return lagrange::compute_components<Scalar, Index>(mesh, blocker_elements_vec, opt);
        },
        "mesh"_a,
        "output_attribute_name"_a = nb::none(),
        "connectivity_type"_a = nb::none(),
        "blocker_elements"_a = nb::none(),
        R"(Compute connected components.

This method will create a per-facet component id attribute named by the `output_attribute_name`
argument. Each component id is in [0, num_components-1] range.

:param mesh: The input mesh.
:param output_attribute_name: The name of the output attribute.
:param connectivity_type: The connectivity type.  Either "Vertex" or "Edge".
:param blocker_elements: The list of blocker element indices. If `connectivity_type` is `Edge`, facets adjacent to a blocker edge are not considered as connected through this edge. If `connectivity_type` is `Vertex`, facets sharing a blocker vertex are not considered as connected through this vertex.

:returns: The total number of components.)");

    nb::class_<VertexValenceOptions>(m, "VertexValenceOptions")
        .def(nb::init<>())
        .def_rw("output_attribute_name", &VertexValenceOptions::output_attribute_name);

    m.def(
        "compute_vertex_valence",
        &lagrange::compute_vertex_valence<Scalar, Index>,
        "mesh"_a,
        "options"_a = VertexValenceOptions());

    nb::class_<TangentBitangentOptions>(m, "TangentBitangentOptions")
        .def(nb::init<>())
        .def_rw("tangent_attribute_name", &TangentBitangentOptions::tangent_attribute_name)
        .def_rw("bitangent_attribute_name", &TangentBitangentOptions::bitangent_attribute_name)
        .def_rw("uv_attribute_name", &TangentBitangentOptions::uv_attribute_name)
        .def_rw("normal_attribute_name", &TangentBitangentOptions::normal_attribute_name)
        .def_rw("output_element_type", &TangentBitangentOptions::output_element_type)
        .def_rw("pad_with_sign", &TangentBitangentOptions::pad_with_sign);

    nb::class_<TangentBitangentResult>(m, "TangentBitangentResult")
        .def(nb::init<>())
        .def_rw("tangent_id", &TangentBitangentResult::tangent_id)
        .def_rw("bitangent_id", &TangentBitangentResult::bitangent_id);

    m.def(
        "compute_tangent_bitangent",
        &lagrange::compute_tangent_bitangent<Scalar, Index>,
        "mesh"_a,
        "options"_a = TangentBitangentOptions());

    m.def(
        "map_attribute",
        static_cast<AttributeId (*)(
            SurfaceMesh<Scalar, Index>&,
            AttributeId,
            std::string_view,
            AttributeElement)>(&lagrange::map_attribute<Scalar, Index>),
        "mesh"_a,
        "old_attribute_id"_a,
        "new_attribute_name"_a,
        "new_element"_a);
    m.def(
        "map_attribute",
        static_cast<AttributeId (*)(
            SurfaceMesh<Scalar, Index>&,
            std::string_view,
            std::string_view,
            AttributeElement)>(&lagrange::map_attribute<Scalar, Index>),
        "mesh"_a,
        "old_attribute_name"_a,
        "new_attribute_name"_a,
        "new_element"_a);
    m.def(
        "map_attribute_in_place",
        static_cast<AttributeId (*)(SurfaceMesh<Scalar, Index>&, AttributeId, AttributeElement)>(
            &map_attribute_in_place<Scalar, Index>),
        "mesh"_a,
        "id"_a,
        "new_element"_a);
    m.def(
        "map_attribute_in_place",
        static_cast<
            AttributeId (*)(SurfaceMesh<Scalar, Index>&, std::string_view, AttributeElement)>(
            &map_attribute_in_place<Scalar, Index>),
        "mesh"_a,
        "name"_a,
        "new_element"_a);
    nb::class_<FacetAreaOptions>(m, "FacetAreaOptions")
        .def(nb::init<>())
        .def_rw("output_attribute_name", &FacetAreaOptions::output_attribute_name);
    m.def(
        "compute_facet_area",
        &lagrange::compute_facet_area<Scalar, Index>,
        "mesh"_a,
        "options"_a = FacetAreaOptions());
    nb::class_<MeshAreaOptions>(m, "MeshAreaOptions")
        .def(nb::init<>())
        .def_rw("input_attribute_name", &MeshAreaOptions::input_attribute_name);
    m.def(
        "compute_mesh_area",
        &lagrange::compute_mesh_area<Scalar, Index>,
        "mesh"_a,
        "options"_a = MeshAreaOptions());
    nb::class_<FacetCentroidOptions>(m, "FacetCentroidOptions")
        .def(nb::init<>())
        .def_rw("output_attribute_name", &FacetCentroidOptions::output_attribute_name);
    m.def(
        "compute_facet_centroid",
        &lagrange::compute_facet_centroid<Scalar, Index>,
        "mesh"_a,
        "options"_a = FacetCentroidOptions());
    nb::enum_<MeshCentroidOptions::WeightingType>(m, "CentroidWeightingType")
        .value("Uniform", MeshCentroidOptions::Uniform)
        .value("Area", MeshCentroidOptions::Area);
    nb::class_<MeshCentroidOptions>(m, "MeshCentroidOptions")
        .def(nb::init<>())
        .def_rw("weighting_type", &MeshCentroidOptions::weighting_type)
        .def_rw(
            "facet_centroid_attribute_name",
            &MeshCentroidOptions::facet_centroid_attribute_name)
        .def_rw("facet_area_attribute_name", &MeshCentroidOptions::facet_area_attribute_name);
    m.def(
        "compute_mesh_centroid",
        [](const SurfaceMesh<Scalar, Index>& mesh, MeshCentroidOptions opt) {
            const Index dim = mesh.get_dimension();
            std::vector<Scalar> centroid(dim, invalid<Scalar>());
            compute_mesh_centroid<Scalar, Index>(mesh, centroid, opt);
            return centroid;
        },
        "mesh"_a,
        "options"_a = MeshCentroidOptions());
    m.def(
        "permute_vertices",
        [](MeshType& mesh, Tensor<Index> new_to_old) {
            auto [data, shape, stride] = tensor_to_span(new_to_old);
            la_runtime_assert(is_dense(shape, stride));
            permute_vertices<Scalar, Index>(mesh, data);
        },
        "mesh"_a,
        "new_to_old"_a,
        R"(Reorder vertices of a mesh in place based on a permutation.

:param mesh: input mesh
:param new_to_old: permutation vector for vertices)");
    m.def(
        "permute_facets",
        [](MeshType& mesh, Tensor<Index> new_to_old) {
            auto [data, shape, stride] = tensor_to_span(new_to_old);
            la_runtime_assert(is_dense(shape, stride));
            permute_facets<Scalar, Index>(mesh, data);
        },
        "mesh"_a,
        "new_to_old"_a,
        R"(Reorder facets of a mesh in place based on a permutation.

:param mesh: input mesh
:param new_to_old: permutation vector for facets)");

    nb::enum_<MappingPolicy>(m, "MappingPolicy")
        .value("Average", MappingPolicy::Average)
        .value("KeepFirst", MappingPolicy::KeepFirst)
        .value("Error", MappingPolicy::Error);
    nb::class_<RemapVerticesOptions>(m, "RemapVerticesOptions")
        .def(nb::init<>())
        .def_rw("collision_policy_float", &RemapVerticesOptions::collision_policy_float)
        .def_rw("collision_policy_integral", &RemapVerticesOptions::collision_policy_integral);
    m.def(
        "remap_vertices",
        [](MeshType& mesh, Tensor<Index> old_to_new, RemapVerticesOptions opt) {
            auto [data, shape, stride] = tensor_to_span(old_to_new);
            la_runtime_assert(is_dense(shape, stride));
            remap_vertices<Scalar, Index>(mesh, data, opt);
        },
        "mesh"_a,
        "old_to_new"_a,
        "options"_a = RemapVerticesOptions());

    m.def(
        "separate_by_facet_groups",
        [](MeshType& mesh,
           Tensor<Index> facet_group_indices,
           std::string_view source_vertex_attr_name,
           std::string_view source_facet_attr_name,
           bool map_attributes) {
            SeparateByFacetGroupsOptions options;
            options.source_vertex_attr_name = source_vertex_attr_name;
            options.source_facet_attr_name = source_facet_attr_name;
            options.map_attributes = map_attributes;
            auto [data, shape, stride] = tensor_to_span(facet_group_indices);
            la_runtime_assert(is_dense(shape, stride));
            return separate_by_facet_groups<Scalar, Index>(mesh, data, options);
        },
        "mesh"_a,
        "facet_group_indices"_a,
        "source_vertex_attr_name"_a = "",
        "source_facet_attr_name"_a = "",
        "map_attributes"_a = false,
        R"(Extract a set of submeshes based on facet groups.

:param mesh:                    The source mesh.
:param facet_group_indices:     The group index for each facet. Each group index must be in the range of [0, max(facet_group_indices)]
:param source_vertex_attr_name: The optional attribute name to track source vertices.
:param source_facet_attr_name:  The optional attribute name to track source facets.

:returns: A list of meshes, one for each facet group.
)");

    m.def(
        "separate_by_components",
        [](MeshType& mesh,
           std::string_view source_vertex_attr_name,
           std::string_view source_facet_attr_name,
           bool map_attributes,
           ConnectivityType connectivity_type) {
            SeparateByComponentsOptions options;
            options.source_vertex_attr_name = source_vertex_attr_name;
            options.source_facet_attr_name = source_facet_attr_name;
            options.map_attributes = map_attributes;
            options.connectivity_type = connectivity_type;
            return separate_by_components(mesh, options);
        },
        "mesh"_a,
        "source_vertex_attr_name"_a = "",
        "source_facet_attr_name"_a = "",
        "map_attributes"_a = false,
        "connectivity_type"_a = ConnectivityType::Edge,
        R"(Extract a set of submeshes based on connected components.

:param mesh:                    The source mesh.
:param source_vertex_attr_name: The optional attribute name to track source vertices.
:param source_facet_attr_name:  The optional attribute name to track source facets.
:param map_attributes:          Map attributes from the source to target meshes.
:param connectivity_type:       The connectivity used for component computation.

:returns: A list of meshes, one for each connected component.
)");

    m.def(
        "extract_submesh",
        [](MeshType& mesh,
           Tensor<Index> selected_facets,
           std::string_view source_vertex_attr_name,
           std::string_view source_facet_attr_name,
           bool map_attributes) {
            SubmeshOptions options;
            options.source_vertex_attr_name = source_vertex_attr_name;
            options.source_facet_attr_name = source_facet_attr_name;
            options.map_attributes = map_attributes;
            auto [data, shape, stride] = tensor_to_span(selected_facets);
            la_runtime_assert(is_dense(shape, stride));
            return extract_submesh<Scalar, Index>(mesh, data, options);
        },
        "mesh"_a,
        "selected_facets"_a,
        "source_vertex_attr_name"_a = "",
        "source_facet_attr_name"_a = "",
        "map_attributes"_a = false,
        R"(Extract a submesh based on the selected facets.

:param mesh:                    The source mesh.
:param selected_facets:         A listed of facet ids to extract.
:param source_vertex_attr_name: The optional attribute name to track source vertices.
:param source_facet_attr_name:  The optional attribute name to track source facets.
:param map_attributes:          Map attributes from the source to target meshes.

:returns: A mesh that contains only the selected facets.
)");

    m.def(
        "compute_dihedral_angles",
        [](MeshType& mesh,
           std::optional<std::string_view> output_attribute_name,
           std::optional<std::string_view> facet_normal_attribute_name,
           std::optional<bool> recompute_facet_normals,
           std::optional<bool> keep_facet_normals) {
            DihedralAngleOptions options;
            if (output_attribute_name.has_value()) {
                options.output_attribute_name = output_attribute_name.value();
            }
            if (facet_normal_attribute_name.has_value()) {
                options.facet_normal_attribute_name = facet_normal_attribute_name.value();
            }
            if (recompute_facet_normals.has_value()) {
                options.recompute_facet_normals = recompute_facet_normals.value();
            }
            if (keep_facet_normals.has_value()) {
                options.keep_facet_normals = keep_facet_normals.value();
            }
            return compute_dihedral_angles(mesh, options);
        },
        "mesh"_a,
        "output_attribute_name"_a = nb::none(),
        "facet_normal_attribute_name"_a = nb::none(),
        "recompute_facet_normals"_a = nb::none(),
        "keep_facet_normals"_a = nb::none(),
        R"(Compute dihedral angles for each edge.

The dihedral angle of an edge is defined as the angle between the __normals__ of two facets adjacent
to the edge. The dihedral angle is always in the range [0, pi] for manifold edges. For boundary
edges, the dihedral angle defaults to 0.  For non-manifold edges, the dihedral angle is not
well-defined and will be set to the special value 2 * M_PI.

:param mesh:                        The source mesh.
:param output_attribute_name:       The optional edge attribute name to store the dihedral angles.
:param facet_normal_attribute_name: The optional attribute name to store the facet normals.
:param recompute_facet_normals:     Whether to recompute facet normals.
:param keep_facet_normals:          Whether to keep newly computed facet normals. It has no effect on pre-existing facet normals.

:return: The edge attribute id of dihedral angles.)");

    m.def(
        "compute_edge_lengths",
        [](MeshType& mesh, std::optional<std::string_view> output_attribute_name) {
            EdgeLengthOptions options;
            if (output_attribute_name.has_value())
                options.output_attribute_name = output_attribute_name.value();
            return compute_edge_lengths(mesh, options);
        },
        "mesh"_a,
        "output_attribute_name"_a = nb::none(),
        R"(Compute edge lengths.

:param mesh:                  The source mesh.
:param output_attribute_name: The optional edge attribute name to store the edge lengths.

:return: The edge attribute id of edge lengths.)");

    m.def(
        "compute_dijkstra_distance",
        [](MeshType& mesh,
           Index seed_facet,
           const nb::list& barycentric_coords,
           std::optional<Scalar> radius,
           std::string_view output_attribute_name,
           bool output_involved_vertices) {
            DijkstraDistanceOptions<Scalar, Index> options;
            options.seed_facet = seed_facet;
            for (auto val : barycentric_coords) {
                options.barycentric_coords.push_back(nb::cast<Scalar>(val));
            }
            if (radius.has_value()) {
                options.radius = radius.value();
            }
            options.output_attribute_name = output_attribute_name;
            options.output_involved_vertices = output_involved_vertices;
            return compute_dijkstra_distance(mesh, options);
        },
        "mesh"_a,
        "seed_facet"_a,
        "barycentric_coords"_a,
        "radius"_a = nb::none(),
        "output_attribute_name"_a = DijkstraDistanceOptions<Scalar, Index>{}.output_attribute_name,
        "output_involved_vertices"_a =
            DijkstraDistanceOptions<Scalar, Index>{}.output_involved_vertices,
        R"(Compute Dijkstra distance from a seed facet.

:param mesh:                  The source mesh.
:param seed_facet:            The seed facet index.
:param barycentric_coords:    The barycentric coordinates of the seed facet.
:param radius:                The maximum radius of the dijkstra distance.
:param output_attribute_name: The output attribute name to store the dijkstra distance.
:param output_involved_vertices: Whether to output the list of involved vertices.)");

    m.def(
        "weld_indexed_attribute",
        nb::overload_cast<SurfaceMesh<Scalar, Index>&, AttributeId>(
            &weld_indexed_attribute<Scalar, Index>),
        "mesh"_a,
        "attribute_id"_a,
        R"(Weld indexed attribute.

:param mesh:         The source mesh.
:param attribute_id: The indexed attribute id to weld.)");

    m.def("compute_euler", &compute_euler<Scalar, Index>, "mesh"_a);
    m.def("is_vertex_manifold", &is_vertex_manifold<Scalar, Index>, "mesh"_a);
    m.def("is_edge_manifold", &is_edge_manifold<Scalar, Index>, "mesh"_a);
    m.def("is_manifold", &is_manifold<Scalar, Index>, "mesh"_a);

    m.def(
        "transform_mesh",
        [](MeshType& mesh,
           Eigen::Matrix<Scalar, 4, 4> affine_transform,
           bool normalize_normals,
           bool normalize_tangents_bitangents,
           bool in_place) -> std::optional<MeshType> {
            Eigen::Transform<Scalar, 3, Eigen::Affine> M(affine_transform);
            TransformOptions options;
            options.normalize_normals = normalize_normals;
            options.normalize_tangents_bitangents = normalize_tangents_bitangents;

            std::optional<MeshType> result;
            if (in_place) {
                transform_mesh(mesh, M, options);
            } else {
                result = transformed_mesh(mesh, M, options);
            }
            return result;
        },
        "mesh"_a,
        "affine_transform"_a,
        "normalize_normals"_a = true,
        "normalize_tangents_bitangents"_a = true,
        "in_place"_a = true,
        R"(Apply affine transformation to a mesh.

:param mesh:                          The source mesh.
:param affine_transform:              The affine transformation matrix.
:param normalize_normals:             Whether to normalize normals.
:param normalize_tangents_bitangents: Whether to normalize tangents and bitangents.
:param in_place:                      Whether to apply the transformation in place.

:return: The transformed mesh if in_place is False.)");
}

} // namespace lagrange::python

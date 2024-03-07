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
#include <lagrange/compute_uv_distortion.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/compute_vertex_valence.h>
#include <lagrange/extract_submesh.h>
#include <lagrange/filter_attributes.h>
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
#include <nanobind/stl/string.h>
#include <nanobind/stl/variant.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/unordered_set.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <optional>
#include <string_view>
#include <vector>

namespace lagrange::python {

template <typename Scalar, typename Index>
void bind_utilities(nanobind::module_& m)
{
    namespace nb = nanobind;
    using namespace nb::literals;
    using MeshType = SurfaceMesh<Scalar, Index>;

    nb::enum_<NormalWeightingType>(m, "NormalWeightingType", "Normal weighting type.")
        .value("Uniform", NormalWeightingType::Uniform, "Uniform weighting")
        .value(
            "CornerTriangleArea",
            NormalWeightingType::CornerTriangleArea,
            "Weight by corner triangle area")
        .value("Angle", NormalWeightingType::Angle, "Weight by corner angle");

    nb::class_<VertexNormalOptions>(
        m,
        "VertexNormalOptions",
        "Options for computing vertex normals")
        .def(nb::init<>())
        .def_rw(
            "output_attribute_name",
            &VertexNormalOptions::output_attribute_name,
            "Output attribute name. Default is '@vertex_normal'.")
        .def_rw(
            "weight_type",
            &VertexNormalOptions::weight_type,
            "Weighting type for normal computation. Default is Angle.")
        .def_rw(
            "weighted_corner_normal_attribute_name",
            &VertexNormalOptions::weighted_corner_normal_attribute_name,
            "Precomputed weighted corner normals attribute name."
            "Default is '@weighted_corner_normal'. "
            "If attribute exists, the weighted corner normals will be "
            "used instead of recomputing them.")
        .def_rw(
            "recompute_weighted_corner_normals",
            &VertexNormalOptions::recompute_weighted_corner_normals,
            "Whether to recompute weighted corner normals. Default is false")
        .def_rw(
            "keep_weighted_corner_normals",
            &VertexNormalOptions::keep_weighted_corner_normals,
            "Whether to keep the weighted corner normal attirbute. Default is false.");

    m.def(
        "compute_vertex_normal",
        &compute_vertex_normal<Scalar, Index>,
        "mesh"_a,
        "options"_a = VertexNormalOptions(),
        R"(Computer vertex normal.

:param mesh: Input mesh.
:param options: Options for computing vertex normals.

:returns: Vertex normal attribute id.)");

    m.def(
        "compute_vertex_normal",
        [](MeshType& mesh,
           std::optional<std::string_view> output_attribute_name,
           std::optional<NormalWeightingType> weight_type,
           std::optional<std::string_view> weighted_corner_normal_attribute_name,
           std::optional<bool> recompute_weighted_corner_normals,
           std::optional<bool> keep_weighted_corner_normals) {
            VertexNormalOptions options;
            if (output_attribute_name) options.output_attribute_name = *output_attribute_name;
            if (weight_type) options.weight_type = *weight_type;
            if (weighted_corner_normal_attribute_name)
                options.weighted_corner_normal_attribute_name =
                    *weighted_corner_normal_attribute_name;
            if (recompute_weighted_corner_normals)
                options.recompute_weighted_corner_normals = *recompute_weighted_corner_normals;
            if (keep_weighted_corner_normals)
                options.keep_weighted_corner_normals = *keep_weighted_corner_normals;

            return compute_vertex_normal<Scalar, Index>(mesh, options);
        },
        "mesh"_a,
        "output_attribute_name"_a = nb::none(),
        "weight_type"_a = nb::none(),
        "weighted_corner_normal_attribute_name"_a = nb::none(),
        "recompute_weighted_corner_normals"_a = nb::none(),
        "keep_weighted_corner_normals"_a = nb::none(),
        R"(Computer vertex normal (Pythonic API).

:param mesh: Input mesh.
:param output_attribute_name: Output attribute name.
:param weight_type: Weighting type for normal computation.
:param weighted_corner_normal_attribute_name: Precomputed weighted corner normals attribute name.
:param recompute_weighted_corner_normals: Whether to recompute weighted corner normals.
:param keep_weighted_corner_normals: Whether to keep the weighted corner normal attirbute.

:returns: Vertex normal attribute id.)");

    nb::class_<FacetNormalOptions>(m, "FacetNormalOptions", "Facet normal computation options.")
        .def(nb::init<>())
        .def_rw(
            "output_attribute_name",
            &FacetNormalOptions::output_attribute_name,
            "Output attribute name. Default: '@facet_normal'");

    m.def(
        "compute_facet_normal",
        &compute_facet_normal<Scalar, Index>,
        "mesh"_a,
        "options"_a = FacetNormalOptions(),
        R"(Computer facet normal.

:param mesh: Input mesh.
:param options: Options for computing facet normals.

:returns: Facet normal attribute id.)");

    m.def(
        "compute_facet_normal",
        [](MeshType& mesh, std::optional<std::string_view> output_attribute_name) {
            FacetNormalOptions options;
            if (output_attribute_name) options.output_attribute_name = *output_attribute_name;
            return compute_facet_normal<Scalar, Index>(mesh, options);
        },
        "mesh"_a,
        "output_attribute_name"_a = nb::none(),
        R"(Computer facet normal (Pythonic API).

:param mesh: Input mesh.
:param output_attribute_name: Output attribute name.

:returns: Facet normal attribute id.)");

    nb::class_<NormalOptions>(m, "NormalOptions", "Normal computation options.")
        .def(nb::init<>())
        .def_rw(
            "output_attribute_name",
            &NormalOptions::output_attribute_name,
            "Output attribute name. Default: '@normal'")
        .def_rw(
            "weight_type",
            &NormalOptions::weight_type,
            "Weighting type for normal computation. Default is Angle.")
        .def_rw(
            "facet_normal_attribute_name",
            &NormalOptions::facet_normal_attribute_name,
            "Facet normal attribute name to use. Default is '@facet_normal'.")
        .def_rw(
            "recompute_facet_normals",
            &NormalOptions::recompute_facet_normals,
            "Whether to recompute facet normals. Default is false.")
        .def_rw(
            "keep_facet_normals",
            &NormalOptions::keep_facet_normals,
            "Whether to keep the computed facet normal attirbute. Default is false.");

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

    m.def(
        "compute_normal",
        [](MeshType& mesh,
           Scalar feature_angle_threshold,
           nb::object cone_vertices,
           std::optional<std::string_view> output_attribute_name,
           std::optional<NormalWeightingType> weight_type,
           std::optional<std::string_view> facet_normal_attribute_name,
           std::optional<bool> recompute_facet_normals,
           std::optional<bool> keep_facet_normals) {
            NormalOptions options;
            if (output_attribute_name) options.output_attribute_name = *output_attribute_name;
            if (weight_type) options.weight_type = *weight_type;
            if (facet_normal_attribute_name)
                options.facet_normal_attribute_name = *facet_normal_attribute_name;
            if (recompute_facet_normals) options.recompute_facet_normals = *recompute_facet_normals;
            if (keep_facet_normals) options.keep_facet_normals = *keep_facet_normals;

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
        "output_attribute_name"_a = nb::none(),
        "weight_type"_a = nb::none(),
        "facet_normal_attribute_name"_a = nb::none(),
        "recompute_facet_normals"_a = nb::none(),
        "keep_facet_normals"_a = nb::none(),
        R"(Compute indexed normal attribute (Pythonic API).

:param mesh: input mesh
:type mesh: SurfaceMesh
:param feature_angle_threshold: feature angle threshold
:type feature_angle_threshold: float, optional
:param cone_vertices: cone vertices
:type cone_vertices: list[int] or numpy.ndarray, optional
:param output_attribute_name: output normal attribute name
:type output_attribute_name: str, optional
:param weight_type: normal weighting type
:type weight_type: NormalWeightingType, optional
:param facet_normal_attribute_name: facet normal attribute name
:type facet_normal_attribute_name: str, optional
:param recompute_facet_normals: whether to recompute facet normals
:type recompute_facet_normals: bool, optional
:param keep_facet_normals: whether to keep the computed facet normal attirbute
:type keep_facet_normals: bool, optional

:returns: the id of the indexed normal attribute.)");

    m.def(
        "normalize_mesh",
        &normalize_mesh<Scalar, Index>,
        "mesh"_a,
        R"(Normalize a mesh to fit into a unit box centered at the origin.

:param mesh: input mesh)");

    m.def(
        "normalize_meshes",
        [](std::vector<SurfaceMesh<Scalar, Index>*> meshes) {
            span<SurfaceMesh<Scalar, Index>*> meshes_span(meshes.data(), meshes.size());
            normalize_meshes(meshes_span);
        },
        "meshes"_a,
        R"(Normalize a list of meshes to fit into a unit box centered at the origin.

:param meshes: input meshes)");

    m.def(
        "combine_meshes",
        [](std::vector<SurfaceMesh<Scalar, Index>*> meshes, bool preserve_vertices) {
            return combine_meshes<Scalar, Index>(
                meshes.size(),
                [&](size_t i) -> const SurfaceMesh<Scalar, Index>& { return *meshes[i]; },
                preserve_vertices);
        },
        "meshes"_a,
        "preserve_attributes"_a = true,
        R"(Combine a list of meshes into a single mesh.

:param meshes: input meshes
:type meshes: list[SurfaceMesh]
:param preserve_attributes: whether to preserve attributes
:type preserve_attributes: bool

:returns: the combined mesh)");

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
        "mesh"_a,
        R"(Triangulate polygonal facets of the mesh.

:param mesh: The input mesh.)");

    nb::enum_<ComponentOptions::ConnectivityType>(m, "ConnectivityType", "Mesh connectivity type")
        .value(
            "Vertex",
            ComponentOptions::ConnectivityType::Vertex,
            "Two facets are connected if they share a vertex")
        .value(
            "Edge",
            ComponentOptions::ConnectivityType::Edge,
            "Two facets are connected if they share an edge");

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

    nb::class_<VertexValenceOptions>(m, "VertexValenceOptions", "Vertex valence options")
        .def(nb::init<>())
        .def_rw(
            "output_attribute_name",
            &VertexValenceOptions::output_attribute_name,
            "The name of the output attribute");

    m.def(
        "compute_vertex_valence",
        &lagrange::compute_vertex_valence<Scalar, Index>,
        "mesh"_a,
        "options"_a = VertexValenceOptions(),
        R"(Compute vertex valence

:param mesh: The input mesh.
:param options: The vertex valence options.

:returns: The vertex valence attribute id.)");

    m.def(
        "compute_vertex_valance",
        [](MeshType& mesh, std::optional<std::string_view> output_attribute_name) {
            VertexValenceOptions opt;
            if (output_attribute_name.has_value()) {
                opt.output_attribute_name = output_attribute_name.value();
            }
            return lagrange::compute_vertex_valence<Scalar, Index>(mesh, opt);
        },
        "mesh"_a,
        "output_attribute_name"_a = nb::none(),
        R"(Compute vertex valence);

:param mesh: The input mesh.
:param output_attribute_name: The name of the output attribute.

:returns: The vertex valence attribute id)");

    nb::class_<TangentBitangentOptions>(m, "TangentBitangentOptions", "Tangent bitangent options")
        .def(nb::init<>())
        .def_rw(
            "tangent_attribute_name",
            &TangentBitangentOptions::tangent_attribute_name,
            "The name of the output tangent attribute, default is '@tangent'")
        .def_rw(
            "bitangent_attribute_name",
            &TangentBitangentOptions::bitangent_attribute_name,
            "The name of the output bitangent attribute, default is '@bitangent'")
        .def_rw(
            "uv_attribute_name",
            &TangentBitangentOptions::uv_attribute_name,
            "The name of the uv attribute")
        .def_rw(
            "normal_attribute_name",
            &TangentBitangentOptions::normal_attribute_name,
            "The name of the normal attribute")
        .def_rw(
            "output_element_type",
            &TangentBitangentOptions::output_element_type,
            "The output element type")
        .def_rw(
            "pad_with_sign",
            &TangentBitangentOptions::pad_with_sign,
            "Whether to pad the output tangent/bitangent with sign");

    nb::class_<TangentBitangentResult>(m, "TangentBitangentResult", "Tangent bitangent result")
        .def(nb::init<>())
        .def_rw(
            "tangent_id",
            &TangentBitangentResult::tangent_id,
            "The output tangent attribute id")
        .def_rw(
            "bitangent_id",
            &TangentBitangentResult::bitangent_id,
            "The output bitangent attribute id");

    m.def(
        "compute_tangent_bitangent",
        &lagrange::compute_tangent_bitangent<Scalar, Index>,
        "mesh"_a,
        "options"_a = TangentBitangentOptions(),
        R"(Compute tangent and bitangent vector attributes.

:param mesh: The input mesh.
:param options: The tangent bitangent options.

:returns: The tangent and bitangent attribute ids)");

    m.def(
        "compute_tangent_bitangent",
        [](MeshType& mesh,
           std::optional<std::string_view>(tangent_attribute_name),
           std::optional<std::string_view>(bitangent_attribute_name),
           std::optional<std::string_view>(uv_attribute_name),
           std::optional<std::string_view>(normal_attribute_name),
           std::optional<AttributeElement>(output_attribute_type),
           std::optional<bool>(pad_with_sign)) {
            TangentBitangentOptions opt;
            if (tangent_attribute_name.has_value()) {
                opt.tangent_attribute_name = tangent_attribute_name.value();
            }
            if (bitangent_attribute_name.has_value()) {
                opt.bitangent_attribute_name = bitangent_attribute_name.value();
            }
            if (uv_attribute_name.has_value()) {
                opt.uv_attribute_name = uv_attribute_name.value();
            }
            if (normal_attribute_name.has_value()) {
                opt.normal_attribute_name = normal_attribute_name.value();
            }
            if (output_attribute_type.has_value()) {
                opt.output_element_type = output_attribute_type.value();
            }
            if (pad_with_sign.has_value()) {
                opt.pad_with_sign = pad_with_sign.value();
            }

            auto r = compute_tangent_bitangent<Scalar, Index>(mesh, opt);
            return std::make_tuple(r.tangent_id, r.bitangent_id);
        },
        "mesh"_a,
        "tangent_attribute_name"_a = nb::none(),
        "bitangent_attribute_name"_a = nb::none(),
        "uv_attribute_name"_a = nb::none(),
        "normal_attribute_name"_a = nb::none(),
        "output_attribute_type"_a = nb::none(),
        "pad_with_sign"_a = nb::none(),
        R"(Compute tangent and bitangent vector attributes (Pythonic API).

:param mesh: The input mesh.
:param tangent_attribute_name: The name of the output tangent attribute.
:param bitangent_attribute_name: The name of the output bitangent attribute.
:param uv_attribute_name: The name of the uv attribute.
:param normal_attribute_name: The name of the normal attribute.
:param output_attribute_type: The output element type.
:param pad_with_sign: Whether to pad the output tangent/bitangent with sign.

:returns: The tangent and bitangent attribute ids)");

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
        "new_element"_a,
        R"(Map an attribute to a new element type.

:param mesh: The input mesh.
:param old_attribute_id: The id of the input attribute.
:param new_attribute_name: The name of the new attribute.
:param new_element: The new element type.

:returns: The id of the new attribute.)");

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
        "new_element"_a,
        R"(Map an attribute to a new element type.

:param mesh: The input mesh.
:param old_attribute_name: The name of the input attribute.
:param new_attribute_name: The name of the new attribute.
:param new_element: The new element type.

:returns: The id of the new attribute.)");

    m.def(
        "map_attribute_in_place",
        static_cast<AttributeId (*)(SurfaceMesh<Scalar, Index>&, AttributeId, AttributeElement)>(
            &map_attribute_in_place<Scalar, Index>),
        "mesh"_a,
        "id"_a,
        "new_element"_a,
        R"(Map an attribute to a new element type in place.

:param mesh: The input mesh.
:param id: The id of the input attribute.
:param new_element: The new element type.

:returns: The id of the new attribute.)");

    m.def(
        "map_attribute_in_place",
        static_cast<
            AttributeId (*)(SurfaceMesh<Scalar, Index>&, std::string_view, AttributeElement)>(
            &map_attribute_in_place<Scalar, Index>),
        "mesh"_a,
        "name"_a,
        "new_element"_a,
        R"(Map an attribute to a new element type in place.

:param mesh: The input mesh.
:param name: The name of the input attribute.
:param new_element: The new element type.

:returns: The id of the new attribute.)");

    nb::class_<FacetAreaOptions>(m, "FacetAreaOptions", "Options for computing facet area.")
        .def(nb::init<>())
        .def_rw(
            "output_attribute_name",
            &FacetAreaOptions::output_attribute_name,
            "The name of the output attribute.");

    m.def(
        "compute_facet_area",
        &lagrange::compute_facet_area<Scalar, Index>,
        "mesh"_a,
        "options"_a = FacetAreaOptions(),
        R"(Compute facet area.

:param mesh: The input mesh.
:param options: The options for computing facet area.

:returns: The id of the new attribute.)");

    m.def(
        "compute_facet_area",
        [](MeshType& mesh, std::optional<std::string_view> name) {
            FacetAreaOptions opt;
            if (name.has_value()) {
                opt.output_attribute_name = name.value();
            }
            return lagrange::compute_facet_area<Scalar, Index>(mesh, opt);
        },
        "mesh"_a,
        "output_attribute_name"_a = nb::none(),
        R"(Compute facet area (Pythonic API).

:param mesh: The input mesh.
:param output_attribute_name: The name of the output attribute.

:returns: The id of the new attribute.)");

    nb::class_<MeshAreaOptions>(m, "MeshAreaOptions", "Options for computing mesh area.")
        .def(nb::init<>())
        .def_rw(
            "input_attribute_name",
            &MeshAreaOptions::input_attribute_name,
            "The name of the pre-computed facet area attribute, default is '@facet_area'.")
        .def_rw(
            "use_signed_area",
            &MeshAreaOptions::use_signed_area,
            "Whether to use signed area.");

    m.def(
        "compute_mesh_area",
        &lagrange::compute_mesh_area<Scalar, Index>,
        "mesh"_a,
        "options"_a = MeshAreaOptions(),
        R"(Compute mesh area.

:param mesh: The input mesh.
:param options: The options for computing mesh area.

:returns: The mesh area.)");

    m.def(
        "compute_mesh_area",
        [](MeshType& mesh,
           std::optional<std::string_view> input_attribute_name,
           std::optional<bool> use_signed_area) {
            MeshAreaOptions opt;
            if (input_attribute_name.has_value()) {
                opt.input_attribute_name = input_attribute_name.value();
            }
            if (use_signed_area.has_value()) {
                opt.use_signed_area = use_signed_area.value();
            }
            return compute_mesh_area(mesh, opt);
        },
        "mesh"_a,
        "input_attribute_name"_a = nb::none(),
        "use_signed_area"_a = nb::none(),
        R"(Compute mesh area (Pythonic API).

:param mesh: The input mesh.
:param input_attribute_name: The name of the pre-computed facet area attribute.
:param use_signed_area: Whether to use signed area.

:returns: The mesh area.)");

    nb::class_<FacetCentroidOptions>(m, "FacetCentroidOptions", "Facet centroid options.")
        .def(nb::init<>())
        .def_rw(
            "output_attribute_name",
            &FacetCentroidOptions::output_attribute_name,
            "The name of the output attribute.");
    m.def(
        "compute_facet_centroid",
        &lagrange::compute_facet_centroid<Scalar, Index>,
        "mesh"_a,
        "options"_a = FacetCentroidOptions(),
        R"(Compute facet centroid.

:param mesh: The input mesh.
:param options: The options for computing facet centroid.

:returns: The id of the new attribute.)");

    m.def(
        "compute_facet_centroid",
        [](MeshType& mesh, std::optional<std::string_view> output_attribute_name) {
            FacetCentroidOptions opt;
            if (output_attribute_name.has_value()) {
                opt.output_attribute_name = output_attribute_name.value();
            }
            return lagrange::compute_facet_centroid<Scalar, Index>(mesh, opt);
        },
        "mesh"_a,
        "output_attribute_name"_a = nb::none(),
        R"(Compute facet centroid (Pythonic API).

:param mesh: The input mesh.
:param output_attribute_name: The name of the output attribute.

:returns: The id of the new attribute.)");

    nb::enum_<MeshCentroidOptions::WeightingType>(
        m,
        "CentroidWeightingType",
        "Centroid weighting type.")
        .value("Uniform", MeshCentroidOptions::Uniform, "Uniform weighting.")
        .value("Area", MeshCentroidOptions::Area, "Area weighting.");

    nb::class_<MeshCentroidOptions>(m, "MeshCentroidOptions", "Mesh centroid options.")
        .def(nb::init<>())
        .def_rw("weighting_type", &MeshCentroidOptions::weighting_type, "The weighting type.")
        .def_rw(
            "facet_centroid_attribute_name",
            &MeshCentroidOptions::facet_centroid_attribute_name,
            "The name of the pre-computed facet centroid attribute if available.")
        .def_rw(
            "facet_area_attribute_name",
            &MeshCentroidOptions::facet_area_attribute_name,
            "The name of the pre-computed facet area attribute if available.");

    m.def(
        "compute_mesh_centroid",
        [](const SurfaceMesh<Scalar, Index>& mesh, MeshCentroidOptions opt) {
            const Index dim = mesh.get_dimension();
            std::vector<Scalar> centroid(dim, invalid<Scalar>());
            compute_mesh_centroid<Scalar, Index>(mesh, centroid, opt);
            return centroid;
        },
        "mesh"_a,
        "options"_a = MeshCentroidOptions(),
        R"(Compute mesh centroid.

:param mesh: The input mesh.
:param options: The options for computing mesh centroid.

:returns: The mesh centroid.)");

    m.def(
        "compute_mesh_centroid",
        [](MeshType& mesh,
           std::optional<MeshCentroidOptions::WeightingType> weighting_type,
           std::optional<std::string_view> facet_centroid_attribute_name,
           std::optional<std::string_view> facet_area_attribute_name) {
            MeshCentroidOptions opt;
            if (weighting_type.has_value()) {
                opt.weighting_type = weighting_type.value();
            }
            if (facet_centroid_attribute_name.has_value()) {
                opt.facet_centroid_attribute_name = facet_centroid_attribute_name.value();
            }
            if (facet_area_attribute_name.has_value()) {
                opt.facet_area_attribute_name = facet_area_attribute_name.value();
            }
            const Index dim = mesh.get_dimension();
            std::vector<Scalar> centroid(dim, invalid<Scalar>());
            compute_mesh_centroid<Scalar, Index>(mesh, centroid, opt);
            return centroid;
        },
        "mesh"_a,
        "weighting_type"_a = nb::none(),
        "facet_centroid_attribute_name"_a = nb::none(),
        "facet_area_attribute_name"_a = nb::none(),
        R"(Compute mesh centroid (Pythonic API).

:param mesh: The input mesh.
:param weighting_type: The weighting type. Default is `Area`.
:param facet_centroid_attribute_name: The name of the pre-computed facet centroid attribute if available. Default is '@facet_centroid'.
:param facet_area_attribute_name: The name of the pre-computed facet area attribute if available. Default is '@facet_area'.

:returns: The mesh centroid.)");

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

    nb::enum_<MappingPolicy>(m, "MappingPolicy", "Mapping policy for handling collisions.")
        .value("Average", MappingPolicy::Average, "Compute the average of the collided values.")
        .value("KeepFirst", MappingPolicy::KeepFirst, "Keep the first collided value.")
        .value("Error", MappingPolicy::Error, "Throw an error when collision happens.");

    nb::class_<RemapVerticesOptions>(m, "RemapVerticesOptions", "Options for remapping vertices.")
        .def(nb::init<>())
        .def_rw(
            "collision_policy_float",
            &RemapVerticesOptions::collision_policy_float,
            "The collision policy for float attributes.")
        .def_rw(
            "collision_policy_integral",
            &RemapVerticesOptions::collision_policy_integral,
            "The collision policy for integral attributes.");

    m.def(
        "remap_vertices",
        [](MeshType& mesh, Tensor<Index> old_to_new, RemapVerticesOptions opt) {
            auto [data, shape, stride] = tensor_to_span(old_to_new);
            la_runtime_assert(is_dense(shape, stride));
            remap_vertices<Scalar, Index>(mesh, data, opt);
        },
        "mesh"_a,
        "old_to_new"_a,
        "options"_a = RemapVerticesOptions(),
        R"(Remap vertices of a mesh in place based on a permutation.

:param mesh: input mesh
:param old_to_new: permutation vector for vertices
:param options: options for remapping vertices)");

    m.def(
        "remap_vertices",
        [](MeshType& mesh,
           Tensor<Index> old_to_new,
           std::optional<MappingPolicy> collision_policy_float,
           std::optional<MappingPolicy> collision_policy_integral) {
            RemapVerticesOptions opt;
            if (collision_policy_float.has_value()) {
                opt.collision_policy_float = collision_policy_float.value();
            }
            if (collision_policy_integral.has_value()) {
                opt.collision_policy_integral = collision_policy_integral.value();
            }
            auto [data, shape, stride] = tensor_to_span(old_to_new);
            la_runtime_assert(is_dense(shape, stride));
            remap_vertices<Scalar, Index>(mesh, data, opt);
        },
        "mesh"_a,
        "old_to_new"_a,
        "collision_policy_float"_a = nb::none(),
        "collision_policy_integral"_a = nb::none(),
        R"(Remap vertices of a mesh in place based on a permutation (Pythonic API).

:param mesh: input mesh
:param old_to_new: permutation vector for vertices
:param collision_policy_float: The collision policy for float attributes.
:param collision_policy_integral: The collision policy for integral attributes.)");

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

    m.def(
        "compute_euler",
        &compute_euler<Scalar, Index>,
        "mesh"_a,
        R"(Compute the Euler characteristic.

:param mesh: The source mesh.

:return: The Euler characteristic.)");

    m.def(
        "is_vertex_manifold",
        &is_vertex_manifold<Scalar, Index>,
        "mesh"_a,
        R"(Check if the mesh is vertex manifold.

:param mesh: The source mesh.

:return: Whether the mesh is vertex manifold.)");

    m.def(
        "is_edge_manifold",
        &is_edge_manifold<Scalar, Index>,
        "mesh"_a,
        R"(Check if the mesh is edge manifold.

:param mesh: The source mesh.

:return: Whether the mesh is edge manifold.)");

    m.def("is_manifold", &is_manifold<Scalar, Index>, "mesh"_a, R"(Check if the mesh is manifold.

A mesh considered as manifold if it is both vertex and edge manifold.

:param mesh: The source mesh.

:return: Whether the mesh is manifold.)");

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

    nb::enum_<DistortionMetric>(m, "DistortionMetric", "Distortion metric.")
        .value("Dirichlet", DistortionMetric::Dirichlet, "Dirichlet energy")
        .value("InverseDirichlet", DistortionMetric::InverseDirichlet, "Inverse Dirichlet energy")
        .value(
            "SymmetricDirichlet",
            DistortionMetric::SymmetricDirichlet,
            "Symmetric Dirichlet energy")
        .value("AreaRatio", DistortionMetric::AreaRatio, "Area ratio")
        .value("MIPS", DistortionMetric::MIPS, "Most isotropic parameterization energy");

    m.def(
        "compute_uv_distortion",
        [](MeshType& mesh,
           std::string_view uv_attribute_name,
           std::string_view output_attribute_name,
           DistortionMetric metric) {
            UVDistortionOptions opt;
            opt.uv_attribute_name = uv_attribute_name;
            opt.output_attribute_name = output_attribute_name;
            opt.metric = metric;
            return compute_uv_distortion(mesh, opt);
        },
        "mesh"_a,
        "uv_attribute_name"_a = "@uv",
        "output_attribute_name"_a = "@uv_measure",
        "metric"_a = lagrange::DistortionMetric::MIPS,
        R"(Compute UV distortion.

:param mesh:                  The source mesh.
:param uv_attribute_name:     The input UV attribute name. Default is "@uv".
:param output_attribute_name: The output attribute name to store the distortion. Default is "@uv_measure".
:param metric:                The distortion metric. Default is MIPS.

:return: The facet attribute id for distortion.)");

    using AttributeNameOrId = AttributeFilter::AttributeNameOrId;
    m.def(
        "filter_attributes",
        [](MeshType& mesh,
           std::optional<std::vector<AttributeNameOrId>> included_attributes,
           std::optional<std::vector<AttributeNameOrId>> excluded_attributes,
           std::optional<std::unordered_set<AttributeUsage>> included_usages,
           std::optional<std::unordered_set<AttributeElement>> included_element_types) {
            AttributeFilter filter;
            if (included_attributes.has_value()) {
                filter.included_attributes = included_attributes.value();
            }
            if (excluded_attributes.has_value()) {
                filter.excluded_attributes = excluded_attributes.value();
            }
            if (included_usages.has_value()) {
                filter.included_usages.clear_all();
                for (auto usage : included_usages.value()) {
                    filter.included_usages.set(usage);
                }
            }
            if (included_element_types.has_value()) {
                filter.included_element_types.clear_all();
                for (auto element_type : included_element_types.value()) {
                    filter.included_element_types.set(element_type);
                }
            }
            return filter_attributes(mesh, filter);
        },
        "mesh"_a,
        "included_attributes"_a = nb::none(),
        "excluded_attributes"_a = nb::none(),
        "included_usages"_a = nb::none(),
        "included_element_types"_a = nb::none(),
        R"(Filters the attributes of mesh according to user specifications.

:param mesh: Input mesh.
:param included_attributes: List of attribute names or ids to include. By default, all attributes are included.
:param excluded_attributes: List of attribute names or ids to exclude. By default, no attribute is excluded.
:param included_usages: List of attribute usages to include. By default, all usages are included.
:param included_element_types: List of attribute element types to include. By default, all element types are included.)");
}

} // namespace lagrange::python

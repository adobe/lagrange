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

#include <lagrange/NormalWeightingType.h>
#include <lagrange/combine_meshes.h>
#include <lagrange/compute_centroid.h>
#include <lagrange/compute_components.h>
#include <lagrange/compute_area.h>
#include <lagrange/compute_facet_normal.h>
#include <lagrange/compute_normal.h>
#include <lagrange/compute_tangent_bitangent.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/compute_vertex_valence.h>
#include <lagrange/map_attribute.h>
#include <lagrange/normalize_meshes.h>
#include <lagrange/permute_vertices.h>
#include <lagrange/python/tensor_utils.h>
#include <lagrange/python/utils/StackVector.h>
#include <lagrange/remap_vertices.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/unify_index_buffer.h>
#include <lagrange/utils/invalid.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanobind/nanobind.h>
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
        .def_readwrite("output_attribute_name", &VertexNormalOptions::output_attribute_name)
        .def_readwrite("weight_type", &VertexNormalOptions::weight_type)
        .def_readwrite(
            "weighted_corner_normal_attribute_name",
            &VertexNormalOptions::weighted_corner_normal_attribute_name)
        .def_readwrite(
            "recompute_weighted_corner_normals",
            &VertexNormalOptions::recompute_weighted_corner_normals)
        .def_readwrite(
            "keep_weighted_corner_normals",
            &VertexNormalOptions::keep_weighted_corner_normals);

    m.def(
        "compute_vertex_normal",
        &compute_vertex_normal<Scalar, Index>,
        "mesh"_a,
        "options"_a = VertexNormalOptions());

    nb::class_<FacetNormalOptions>(m, "FacetNormalOptions")
        .def(nb::init<>())
        .def_readwrite("output_attribute_name", &FacetNormalOptions::output_attribute_name);

    m.def(
        "compute_facet_normal",
        &compute_facet_normal<Scalar, Index>,
        "mesh"_a,
        "options"_a = FacetNormalOptions());

    nb::class_<NormalOptions>(m, "NormalOptions")
        .def(nb::init<>())
        .def_readwrite("output_attribute_name", &NormalOptions::output_attribute_name)
        .def_readwrite("weight_type", &NormalOptions::weight_type)
        .def_readwrite("facet_normal_attribute_name", &NormalOptions::facet_normal_attribute_name)
        .def_readwrite("recompute_facet_normals", &NormalOptions::recompute_facet_normals)
        .def_readwrite("keep_facet_normals", &NormalOptions::keep_facet_normals);

    m.def(
        "compute_normal",
        [](MeshType& mesh,
           Scalar feature_angle_threshold,
           Tensor<Index> cone_vertices,
           NormalOptions options) {
            auto [data, shape, stride] = tensor_to_span(cone_vertices);
            la_runtime_assert(is_dense(shape, stride));
            return compute_normal<Scalar, Index>(mesh, feature_angle_threshold, data, options);
        },
        "mesh"_a,
        "feature_angle_threshold"_a = M_PI / 4,
        "cone_vertices"_a,
        "options"_a = NormalOptions());
    // Overload to support python list and default empty array.
    m.def(
        "compute_normal",
        [](MeshType& mesh,
           Scalar feature_angle_threshold,
           const std::vector<Index>& cone_vertices,
           NormalOptions options) {
            span<const Index> data{cone_vertices.data(), cone_vertices.size()};
            return compute_normal<Scalar, Index>(mesh, feature_angle_threshold, data, options);
        },
        "mesh"_a,
        "feature_angle_threshold"_a = M_PI / 4,
        "cone_vertices"_a = std::vector<Index>(),
        "options"_a = NormalOptions());

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
        &lagrange::unify_index_buffer<Scalar, Index>,
        "mesh"_a,
        "attribute_ids"_a);
    m.def(
        "unify_index_buffer",
        &lagrange::unify_named_index_buffer<Scalar, Index>,
        "mesh"_a,
        "attribute_names"_a);
    m.def(
        "triangulate_polygonal_facets",
        &lagrange::triangulate_polygonal_facets<Scalar, Index>,
        "mesh"_a);

    nb::enum_<ComponentOptions::ConnectivityType>(m, "ConnectivityType")
        .value("Vertex", ComponentOptions::ConnectivityType::Vertex)
        .value("Edge", ComponentOptions::ConnectivityType::Edge);

    nb::class_<ComponentOptions>(m, "ComponentOptions")
        .def(nb::init<>())
        .def_readwrite("output_attribute_name", &ComponentOptions::output_attribute_name)
        .def_readwrite("connectivity_type", &ComponentOptions::connectivity_type);

    m.def(
        "compute_components",
        &lagrange::compute_components<Scalar, Index>,
        "mesh"_a,
        "options"_a = ComponentOptions());

    nb::class_<VertexValenceOptions>(m, "VertexValenceOptions")
        .def(nb::init<>())
        .def_readwrite("output_attribute_name", &VertexValenceOptions::output_attribute_name);

    m.def(
        "compute_vertex_valence",
        &lagrange::compute_vertex_valence<Scalar, Index>,
        "mesh"_a,
        "options"_a = VertexValenceOptions());

    nb::class_<TangentBitangentOptions>(m, "TangentBitangentOptions")
        .def(nb::init<>())
        .def_readwrite("tangent_attribute_name", &TangentBitangentOptions::tangent_attribute_name)
        .def_readwrite(
            "bitangent_attribute_name",
            &TangentBitangentOptions::bitangent_attribute_name)
        .def_readwrite("uv_attribute_name", &TangentBitangentOptions::uv_attribute_name)
        .def_readwrite("normal_attribute_name", &TangentBitangentOptions::normal_attribute_name)
        .def_readwrite("output_element_type", &TangentBitangentOptions::output_element_type)
        .def_readwrite("pad_with_sign", &TangentBitangentOptions::pad_with_sign);

    nb::class_<TangentBitangentResult>(m, "TangentBitangentResult")
        .def(nb::init<>())
        .def_readwrite("tangent_id", &TangentBitangentResult::tangent_id)
        .def_readwrite("bitangent_id", &TangentBitangentResult::bitangent_id);

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
        .def_readwrite("output_attribute_name", &FacetAreaOptions::output_attribute_name);
    m.def(
        "compute_facet_area",
        &lagrange::compute_facet_area<Scalar, Index>,
        "mesh"_a,
        "options"_a = FacetAreaOptions());
    nb::class_<MeshAreaOptions>(m, "MeshAreaOptions")
        .def(nb::init<>())
        .def_readwrite("input_attribute_name", &MeshAreaOptions::input_attribute_name);
    m.def(
        "compute_mesh_area",
        &lagrange::compute_mesh_area<Scalar, Index>,
        "mesh"_a,
        "options"_a = MeshAreaOptions());
    nb::class_<FacetCentroidOptions>(m, "FacetCentroidOptions")
        .def(nb::init<>())
        .def_readwrite("output_attribute_name", &FacetCentroidOptions::output_attribute_name);
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
        .def_readwrite("weighting_type", &MeshCentroidOptions::weighting_type)
        .def_readwrite(
            "facet_centroid_attribute_name",
            &MeshCentroidOptions::facet_centroid_attribute_name)
        .def_readwrite(
            "facet_area_attribute_name",
            &MeshCentroidOptions::facet_area_attribute_name);
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
        "new_to_old"_a);

    nb::enum_<RemapVerticesOptions::CollisionPolicy>(m, "CollisionPolicy")
        .value("Average", RemapVerticesOptions::CollisionPolicy::Average)
        .value("KeepFirst", RemapVerticesOptions::CollisionPolicy::KeepFirst)
        .value("Error", RemapVerticesOptions::CollisionPolicy::Error);
    nb::class_<RemapVerticesOptions>(m, "RemapVerticesOptions")
        .def(nb::init<>())
        .def_readwrite("collision_policy_float", &RemapVerticesOptions::collision_policy_float)
        .def_readwrite("collision_policy_integral", &RemapVerticesOptions::collision_policy_integral);
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
}

} // namespace lagrange::python

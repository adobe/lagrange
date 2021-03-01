/*
 * Copyright 2020 Adobe. All rights reserved.
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

#include <Eigen/Dense>
#include <exception>

#include <lagrange/Logger.h>
#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/utils/range.h>

namespace lagrange {

namespace combine_mesh_list_internal {

template <typename MeshTypePtr, typename MeshType2>
void combine_all_vertex_attributes(
    const std::vector<MeshTypePtr>& mesh_list,
    MeshType2& combined_mesh);

template <typename MeshTypePtr, typename MeshType2>
void combine_all_facet_attributes(
    const std::vector<MeshTypePtr>& mesh_list,
    MeshType2& combined_mesh);

template <typename MeshTypePtr, typename MeshType2>
void combine_all_corner_attributes(
    const std::vector<MeshTypePtr>& mesh_list,
    MeshType2& combined_mesh);

template <typename MeshTypePtr, typename MeshType2>
void combine_all_edge_attributes(
    const std::vector<MeshTypePtr>& mesh_list,
    MeshType2& combined_mesh);

template <typename MeshTypePtr, typename MeshType2>
void combine_all_edge_attributes_new(
    const std::vector<MeshTypePtr>& mesh_list,
    MeshType2& combined_mesh);

template <typename MeshTypePtr, typename MeshType2>
void combine_all_indexed_attributes(
    const std::vector<MeshTypePtr>& mesh_list,
    MeshType2& combined_mesh);

} // namespace combine_mesh_list_internal

/**
 * Combines vector of meshes (represented by any pointer type, smart or raw)
 * Returns unique_ptr to combined mesh
 *
 * All valid vertex/facet/corner/edge attributes are combined and forwarded to
 * the output mesh.  An attribute is considered as valid iff it is set for all
 * meshes in `mesh_list`.
 */
template <typename MeshTypePtr>
std::unique_ptr<typename std::pointer_traits<MeshTypePtr>::element_type> combine_mesh_list(
    const std::vector<MeshTypePtr>& mesh_list,
    bool preserve_attributes = false)
{
    static_assert(MeshTrait<MeshTypePtr>::is_mesh_ptr(), "Input type is not Mesh smart ptr");
    using MeshType = typename std::pointer_traits<MeshTypePtr>::element_type;
    using Index = typename MeshType::Index;
    using Vtype = typename MeshType::VertexArray;
    using Ftype = typename MeshType::FacetArray;

    if (mesh_list.size() == 0) return nullptr;

    const auto& front_mesh = mesh_list.front();
    const Index dim = front_mesh->get_dim();
    const Index vertex_per_facet = front_mesh->get_vertex_per_facet();
    Index total_vertices(0), total_facets(0);

    for (const auto& mesh : mesh_list) {
        total_vertices += mesh->get_num_vertices();
        total_facets += mesh->get_num_facets();
    }

    Vtype V(total_vertices, dim);
    Ftype F(total_facets, vertex_per_facet);
    Index curr_v_index(0), curr_f_index(0);

    for (const auto& mesh : mesh_list) {
        assert(mesh->get_dim() == dim);
        assert(mesh->get_vertex_per_facet() == vertex_per_facet);

        V.block(curr_v_index, 0, mesh->get_num_vertices(), dim) = mesh->get_vertices();
        F.block(curr_f_index, 0, mesh->get_num_facets(), vertex_per_facet) =
            mesh->get_facets().array() + curr_v_index;

        curr_v_index += mesh->get_num_vertices();
        curr_f_index += mesh->get_num_facets();
    }

    auto combined_mesh = lagrange::create_mesh(std::move(V), std::move(F));

    if (preserve_attributes) {
        using namespace combine_mesh_list_internal;
        combine_all_vertex_attributes(mesh_list, *combined_mesh);
        combine_all_facet_attributes(mesh_list, *combined_mesh);
        combine_all_corner_attributes(mesh_list, *combined_mesh);
#ifdef LA_KEEP_TRANSITION_CODE
        combine_all_edge_attributes(mesh_list, *combined_mesh);
#endif
        combine_all_edge_attributes_new(mesh_list, *combined_mesh);
        combine_all_indexed_attributes(mesh_list, *combined_mesh);
    }

    return combined_mesh;
}

namespace combine_mesh_list_internal {

template <typename MeshTypePtr, typename MeshType2>
void combine_all_vertex_attributes(
    const std::vector<MeshTypePtr>& mesh_list,
    MeshType2& combined_mesh)
{
    using MeshType = typename std::pointer_traits<MeshTypePtr>::element_type;
    using Index = typename MeshType::Index;
    using AttributeArray = typename MeshType::AttributeArray;

    const auto& front_mesh = mesh_list.front();
    const auto total_num_vertices = combined_mesh.get_num_vertices();

    for (const auto& attr_name : front_mesh->get_vertex_attribute_names()) {
        bool can_merge = true;
        for (const auto& mesh : mesh_list) {
            if (!mesh->has_vertex_attribute(attr_name)) {
                logger().warn("Cannot combine attribute \"{}\"", attr_name);
                can_merge = false;
                break;
            }
        }
        if (!can_merge) continue;

        const auto attribute_dim = front_mesh->get_vertex_attribute(attr_name).cols();
        AttributeArray attr(total_num_vertices, attribute_dim);

        Index curr_row = 0;
        for (const auto& mesh : mesh_list) {
            attr.block(curr_row, 0, mesh->get_num_vertices(), attribute_dim) =
                mesh->get_vertex_attribute(attr_name);
            curr_row += mesh->get_num_vertices();
        }

        combined_mesh.add_vertex_attribute(attr_name);
        combined_mesh.import_vertex_attribute(attr_name, attr);
    }
}

template <typename MeshTypePtr, typename MeshType2>
void combine_all_facet_attributes(
    const std::vector<MeshTypePtr>& mesh_list,
    MeshType2& combined_mesh)
{
    using MeshType = typename std::pointer_traits<MeshTypePtr>::element_type;
    using Index = typename MeshType::Index;
    using AttributeArray = typename MeshType::AttributeArray;

    const auto& front_mesh = mesh_list.front();
    const auto total_num_facets = combined_mesh.get_num_facets();

    for (const auto& attr_name : front_mesh->get_facet_attribute_names()) {
        bool can_merge = true;
        for (const auto& mesh : mesh_list) {
            if (!mesh->has_facet_attribute(attr_name)) {
                can_merge = false;
                logger().warn("Cannot combine facet attribute \"{}\"", attr_name);
                break;
            }
        }
        if (!can_merge) continue;

        const auto attribute_dim = front_mesh->get_facet_attribute(attr_name).cols();
        AttributeArray attr(total_num_facets, attribute_dim);

        Index curr_row = 0;
        for (const auto& mesh : mesh_list) {
            attr.block(curr_row, 0, mesh->get_num_facets(), attribute_dim) =
                mesh->get_facet_attribute(attr_name);
            curr_row += mesh->get_num_facets();
        }

        combined_mesh.add_facet_attribute(attr_name);
        combined_mesh.import_facet_attribute(attr_name, attr);
    }
}

template <typename MeshTypePtr, typename MeshType2>
void combine_all_corner_attributes(
    const std::vector<MeshTypePtr>& mesh_list,
    MeshType2& combined_mesh)
{
    using MeshType = typename std::pointer_traits<MeshTypePtr>::element_type;
    using Index = typename MeshType::Index;
    using AttributeArray = typename MeshType::AttributeArray;

    const auto& front_mesh = mesh_list.front();
    const auto total_num_facets = combined_mesh.get_num_facets();
    const auto vertex_per_facet = combined_mesh.get_vertex_per_facet();

    for (const auto& attr_name : front_mesh->get_corner_attribute_names()) {
        bool can_merge = true;
        for (const auto& mesh : mesh_list) {
            if (!mesh->has_corner_attribute(attr_name)) {
                logger().warn("Cannot combine corner attribute \"{}\"", attr_name);
                can_merge = false;
                break;
            }
        }
        if (!can_merge) continue;

        const auto attribute_dim = front_mesh->get_corner_attribute(attr_name).cols();
        AttributeArray attr(total_num_facets * vertex_per_facet, attribute_dim);

        Index curr_row = 0;
        for (const auto& mesh : mesh_list) {
            const auto num_facets = mesh->get_num_facets();
            attr.block(curr_row, 0, num_facets * vertex_per_facet, attribute_dim) =
                mesh->get_corner_attribute(attr_name);
            curr_row += num_facets * vertex_per_facet;
        }

        combined_mesh.add_corner_attribute(attr_name);
        combined_mesh.import_corner_attribute(attr_name, attr);
    }
}

template <typename MeshTypePtr, typename MeshType2>
void combine_all_edge_attributes(
    const std::vector<MeshTypePtr>& mesh_list,
    MeshType2& combined_mesh)
{
    using MeshType = typename std::pointer_traits<MeshTypePtr>::element_type;
    using Index = typename MeshType::Index;
    using AttributeArray = typename MeshType::AttributeArray;

    const auto& front_mesh = mesh_list.front();

    // All meshes must have edge initialized for this function to work.
    for (const auto& mesh : mesh_list) {
        if (!mesh->is_edge_data_initialized()) return;
    }

    if (!combined_mesh.is_edge_data_initialized()) {
        combined_mesh.initialize_edge_data();
    }

    const auto total_num_edges = combined_mesh.get_num_edges();

    for (const auto& attr_name : front_mesh->get_edge_attribute_names()) {
        bool can_merge = true;
        for (const auto& mesh : mesh_list) {
            if (!mesh->has_edge_attribute(attr_name)) {
                can_merge = false;
                logger().warn("Cannot combine edge attribute \"{}\"", attr_name);
                break;
            }
        }
        if (!can_merge) continue;

        const auto attribute_dim = front_mesh->get_edge_attribute(attr_name).cols();
        AttributeArray attr(total_num_edges, attribute_dim);

        Index index_offset = 0;
        for (const auto& mesh : mesh_list) {
            const auto& edges = mesh->get_edges();
            const auto& per_mesh_attr = mesh->get_edge_attribute(attr_name);
            for (auto i : range(mesh->get_num_edges())) {
                const auto& e = edges[i];
                Index e_idx =
                    combined_mesh.get_edge_index({e[0] + index_offset, e[1] + index_offset});
                attr.row(e_idx) = per_mesh_attr.row(i);
            }
            index_offset += mesh->get_num_vertices();
        }

        combined_mesh.add_edge_attribute(attr_name);
        combined_mesh.import_edge_attribute(attr_name, attr);
    }
}

template <typename MeshTypePtr, typename MeshType2>
void combine_all_edge_attributes_new(
    const std::vector<MeshTypePtr>& mesh_list,
    MeshType2& combined_mesh)
{
    using MeshType = typename std::pointer_traits<MeshTypePtr>::element_type;
    using Index = typename MeshType::Index;
    using AttributeArray = typename MeshType::AttributeArray;

    const auto& front_mesh = mesh_list.front();

    // All meshes must have edge initialized for this function to work.
    for (const auto& mesh : mesh_list) {
        if (!mesh->is_edge_data_initialized_new()) return;
    }

    if (!combined_mesh.is_edge_data_initialized_new()) {
        combined_mesh.initialize_edge_data_new();
    }

    const auto total_num_edges = combined_mesh.get_num_edges_new();

    for (const auto& attr_name : front_mesh->get_edge_attribute_names_new()) {
        bool can_merge = true;
        for (const auto& mesh : mesh_list) {
            if (!mesh->has_edge_attribute_new(attr_name)) {
                can_merge = false;
                logger().warn("Cannot combine edge attribute \"{}\"", attr_name);
                break;
            }
        }
        if (!can_merge) continue;

        const auto attribute_dim = front_mesh->get_edge_attribute_new(attr_name).cols();
        AttributeArray attr(total_num_edges, attribute_dim);

        Index vertex_offset = 0;
        Index facet_offset = 0;
        Index edge_offset = 0;
        for (const auto& mesh : mesh_list) {
            const auto& per_mesh_attr = mesh->get_edge_attribute_new(attr_name);
            for (auto old_e : range(mesh->get_num_edges_new())) {
                const auto& c = mesh->get_one_corner_around_edge_new(old_e);
                const Index f = c / mesh->get_vertex_per_facet();
                const Index lv = c % mesh->get_vertex_per_facet();
                LA_ASSERT_DEBUG(mesh->get_edge_new(f, lv) == old_e);
                const auto new_e = combined_mesh.get_edge_new(f + facet_offset, lv);
                attr.row(new_e) = per_mesh_attr.row(old_e);

                // sanity check
                auto old_v = mesh->get_edge_vertices_new(old_e);
                auto new_v = combined_mesh.get_edge_vertices_new(new_e);
                std::sort(old_v.begin(), old_v.end());
                std::sort(new_v.begin(), new_v.end());
                LA_ASSERT_DEBUG(new_v[0] == old_v[0] + vertex_offset);
                LA_ASSERT_DEBUG(new_v[1] == old_v[1] + vertex_offset);
            }
            vertex_offset += mesh->get_num_vertices();
            facet_offset += mesh->get_num_facets();
            edge_offset += mesh->get_num_edges_new();
        }

        combined_mesh.add_edge_attribute_new(attr_name);
        combined_mesh.import_edge_attribute_new(attr_name, attr);
    }
}

template <typename MeshTypePtr, typename MeshType2>
void combine_all_indexed_attributes(
    const std::vector<MeshTypePtr>& mesh_list,
    MeshType2& combined_mesh)
{
    using MeshType = typename std::pointer_traits<MeshTypePtr>::element_type;
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using AttributeArray = typename MeshType::AttributeArray;
    using IndexArray = typename MeshType::IndexArray;

    const auto& front_mesh = mesh_list.front();

    for (const auto& attr_name : front_mesh->get_indexed_attribute_names()) {
        bool can_merge = true;
        auto ref_attr = front_mesh->get_indexed_attribute_array(attr_name);
        const auto& ref_values = *std::get<0>(ref_attr);
        const auto& ref_indices = *std::get<1>(ref_attr);

        if (ref_values.get_scalar_type() != experimental::ScalarToEnum_v<Scalar>) {
            std::string expected_type = experimental::ScalarToEnum<Scalar>::name;
            std::string current_type = experimental::enum_to_name(ref_values.get_scalar_type());
            logger().warn(
                "Cannot combined indexed attribute ({}) with custom Scalar type \"{}\".  "
                "Expecting \"{}\".", attr_name, current_type, expected_type);
            continue;
        }
        if (ref_indices.get_scalar_type() != experimental::ScalarToEnum_v<Index>) {
            std::string expected_type = experimental::ScalarToEnum<Index>::name;
            std::string current_type = experimental::enum_to_name(ref_indices.get_scalar_type());
            logger().warn(
                "Cannot combined indexed attribute ({}) with custom Index type \"{}\".  "
                "Expecting \"{}\".", attr_name, current_type, expected_type);
            continue;
        }

        Index combined_num_values = 0;
        Index combined_num_indices = 0;

        for (const auto& mesh : mesh_list) {
            if (!mesh->has_indexed_attribute(attr_name)) {
                can_merge = false;
                logger().warn("Cannot combine indexed attribute \"{}\"", attr_name);
                break;
            }
            auto attr = mesh->get_indexed_attribute_array(attr_name);
            const auto& values = *std::get<0>(attr);
            const auto& indices = *std::get<1>(attr);

            if (values.get_scalar_type() != ref_values.get_scalar_type()) {
                can_merge = false;
                logger().warn("Cannot combine indexed attribute because value type mismatch.");
                break;
            }
            if (indices.get_scalar_type() != ref_indices.get_scalar_type()) {
                can_merge = false;
                logger().warn("Cannot combine indexed attribute because index type mismatch.");
                break;
            }

            combined_num_values += safe_cast<Index>(values.rows());
            combined_num_indices += safe_cast<Index>(indices.rows());
        }
        if (!can_merge) continue;

        AttributeArray combined_values(combined_num_values, ref_values.cols());
        IndexArray combined_indices(combined_num_indices, ref_indices.cols());

        Index curr_value_row = 0;
        Index curr_index_row = 0;
        for (const auto& mesh : mesh_list) {
            auto attr = mesh->get_indexed_attribute_array(attr_name);
            const auto& values = *std::get<0>(attr);
            const auto& indices = *std::get<1>(attr);

            combined_values.block(curr_value_row, 0, values.rows(), values.cols()) =
                values.template view<AttributeArray>();
            combined_indices.block(curr_index_row, 0, indices.rows(), indices.cols()) =
                indices.template view<IndexArray>().array() + curr_value_row;

            curr_value_row += safe_cast<Index>(values.rows());
            curr_index_row += safe_cast<Index>(indices.rows());
        }

        combined_mesh.add_indexed_attribute(attr_name);
        combined_mesh.import_indexed_attribute(
            attr_name,
            std::move(combined_values),
            std::move(combined_indices));
    }




}

} // namespace combine_mesh_list_internal

} // namespace lagrange

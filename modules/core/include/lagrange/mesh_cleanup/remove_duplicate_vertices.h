/*
 * Copyright 2016 Adobe. All rights reserved.
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

#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/mesh_cleanup/remove_topologically_degenerate_triangles.h>
#include <lagrange/reorder_mesh_vertices.h>
#include <lagrange/attributes/map_attributes.h>
#include <lagrange/utils/range.h>
#include <lagrange/internal/sortrows.h>

#include <vector>

namespace lagrange {

namespace internal {
// A much lighter version of igl::unique_rows()
template <typename DerivedA, typename Index>
void unique_rows(
    const Eigen::DenseBase<DerivedA>& A,
    Index& num_unique_vertices,
    std::vector<Index>& forward_mapping)
{
    // Sort the rows
    Eigen::VectorXi IM;
    {
        DerivedA __; // sorted A
        constexpr bool is_ascending = true; // does not matter really
        lagrange::internal::sortrows(A, is_ascending, __, IM);
    }

    // Build the forward mapping and the number of unique rows
    const Index num_rows = safe_cast<Index>(A.rows());
    forward_mapping.resize(num_rows);
    num_unique_vertices = 0;
    forward_mapping[IM(0)] = num_unique_vertices;
    for (auto i : range<Index>(1, num_rows)) {
        if ((A.row(IM(i)) != A.row(IM(i - 1)))) {
            ++num_unique_vertices;
        }
        forward_mapping[IM(i)] = static_cast<Index>(num_unique_vertices);
    }
    ++num_unique_vertices; // num_of_xx = last index + 1
}
} // namespace internal


/**
 * Remove duplicated vertices.  Two vertices are duplicates of each other iff
 * they share the same vertex coordinates and have the same attribute values for
 * the specified vertex and indexed attributes.
 *
 * @param[in] mesh                    Input triangle mesh.
 * @param[in] vertex_attribute_names  A vector of vertex attributes that serve as keys.
 * @param[in] indexed_attribute_names A vector of indexed attributes that serve as keys.
 *
 * @return  A new mesh without duplicate vertices.  All attributes are
 *          transferred to the output mesh.
 */
template <typename MeshType>
std::unique_ptr<MeshType> remove_duplicate_vertices(
    const MeshType& mesh,
    const std::vector<std::string>& vertex_attribute_names,
    const std::vector<std::string>& indexed_attribute_names = {})
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    la_runtime_assert(
        mesh.get_vertex_per_facet() == 3,
        std::string("vertex per facet is ") + std::to_string(mesh.get_vertex_per_facet()));

    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using Entries = Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>;

    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();
    const Index num_vertices = mesh.get_num_vertices();
    const Index num_facets = mesh.get_num_facets();

    const Index dim = mesh.get_dim();
    Index num_cols = dim;
    for (const auto& attr_name : vertex_attribute_names) {
        la_runtime_assert(mesh.has_vertex_attribute(attr_name));
        num_cols += static_cast<Index>(mesh.get_vertex_attribute(attr_name).cols());
    }
    for (const auto& attr_name : indexed_attribute_names) {
        la_runtime_assert(mesh.has_indexed_attribute(attr_name));
        auto attr = mesh.get_indexed_attribute(attr_name);
        num_cols += static_cast<Index>(std::get<0>(attr).cols());
    }

    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> vertices_and_keys;
    vertices_and_keys.resize(num_vertices, num_cols);
    vertices_and_keys.leftCols(dim) = vertices;
    Index col_count = dim;

    for (const auto& attr_name : vertex_attribute_names) {
        const auto& attr = mesh.get_vertex_attribute(attr_name);
        vertices_and_keys.block(0, col_count, num_vertices, attr.cols()) = attr;
        col_count += static_cast<Index>(attr.cols());
    }
    for (const auto& attr_name : indexed_attribute_names) {
        const auto attr = mesh.get_indexed_attribute(attr_name);
        const auto& attr_values = std::get<0>(attr);
        const auto& attr_indices = std::get<1>(attr);
        for (auto i : range(num_facets)) {
            vertices_and_keys.row(facets(i, 0)).segment(col_count, attr_values.cols()) =
                attr_values.row(attr_indices(i, 0));
            vertices_and_keys.row(facets(i, 1)).segment(col_count, attr_values.cols()) =
                attr_values.row(attr_indices(i, 1));
            vertices_and_keys.row(facets(i, 2)).segment(col_count, attr_values.cols()) =
                attr_values.row(attr_indices(i, 2));
        }
        col_count += static_cast<Index>(attr_values.cols());
    }

    Entries unique_vertices_and_keys;
    std::vector<Index> forward_mapping;
    Index num_unique_vertices;
    internal::unique_rows(vertices_and_keys, num_unique_vertices, forward_mapping);

    la_runtime_assert(safe_cast<Eigen::Index>(forward_mapping.size()) == vertices.rows());

    if (num_unique_vertices < mesh.get_num_vertices()) {
        auto mesh2 = reorder_mesh_vertices(mesh, forward_mapping);
        mesh2 = remove_topologically_degenerate_triangles(*mesh2);
        return mesh2;
    } else {
        la_runtime_assert(num_unique_vertices == mesh.get_num_vertices());
        auto mesh2 = create_mesh(mesh.get_vertices(), mesh.get_facets());
        map_attributes(mesh, *mesh2);
        return mesh2;
    }
}

/**
 * Remove all duplicate vertices for mesh.
 *
 * Arguments:
 *     input_mesh
 *     key_name: Name of the key vertex attribute.
 *               Only merge two vertices if they have the same coordinates
 *               and the same key value.
 *
 * Returns:
 *     output_mesh without any duplicate vertices.
 *
 * All vertex and facet attributes are mapped from input to the output.
 * The output_mesh's vertices are a subset of input_mesh's vertices.
 * The output_mesh's facets are the same as input_mesh's except vertices
 * maybe reindex.
 */
template <typename MeshType>
std::unique_ptr<MeshType> remove_duplicate_vertices(
    const MeshType& mesh,
    const std::string& key_name = "",
    bool with_uv = true)
{
    std::vector<std::string> vertex_attributes, indexed_attributes;
    if (key_name != "") {
        vertex_attributes.push_back(key_name);
    }
    if (with_uv && mesh.is_uv_initialized()) {
        indexed_attributes.push_back("uv");
    }
    return remove_duplicate_vertices(mesh, vertex_attributes, indexed_attributes);
}

} // namespace lagrange

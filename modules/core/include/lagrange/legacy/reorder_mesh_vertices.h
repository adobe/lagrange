/*
 * Copyright 2019 Adobe. All rights reserved.
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

#include <cassert>

#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/attributes/map_attributes.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/legacy/inline.h>
#include <lagrange/utils/safe_cast.h>

namespace lagrange {
LAGRANGE_LEGACY_INLINE
namespace legacy {

/**
 * Reorders (possibly with shrinking) the vertices in the mesh.
 *
 * Arguments:
 *     input_mesh
 *     forward_mapping: old2new mapping for the vertices in the mesh.
 *       forward_mapping[i] == INVALID or i -> vertex i will remain as is
 *       forward_mapping[i] = j, vertex i will be remapped to j
 *       if two vertices map to the same new index, they will be merged.
 *     ''Forward_mapping must be surjective''
 *
 * Returns:
 *     output_mesh with the vertices merged.
 *
 * NOTES:
 *   All vertex and facet attributes are mapped from input to the output.
 *   The output_mesh's facets are the same as input_mesh's.
 *   The output_mesh's vertices are the same or a subset of the
 *     input_mesh's vertices.
 *   This is not a clean-up routine as-is. DEGENRATE facets can be present
 *     in the result.
 */
template <typename MeshType>
std::unique_ptr<MeshType> reorder_mesh_vertices(
    const MeshType& mesh,
    const typename MeshType::IndexList& forward_mapping)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    la_runtime_assert(
        mesh.get_vertex_per_facet() == 3,
        std::string("vertex per facet is ") + std::to_string(mesh.get_vertex_per_facet()));

    using Index = typename MeshType::Index;
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;
    using IndexList = typename MeshType::IndexList;

    const auto num_old_vertices = mesh.get_num_vertices();
    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();
    la_runtime_assert(num_old_vertices == safe_cast<Index>(forward_mapping.size()));

    auto forward_mapping_no_invalid = [&forward_mapping](const Index io) {
        return forward_mapping[io] == invalid<Index>() ? io : forward_mapping[io];
    };

    // Cound the number of new vertices
    Index num_new_vertices = 0;
    for (const auto io : range(num_old_vertices)) {
        num_new_vertices = std::max(num_new_vertices, forward_mapping_no_invalid(io));
    }
    num_new_vertices += 1; // num_of_*** = biggest index +  1
    la_runtime_assert(
        num_new_vertices <= num_old_vertices,
        "Number of vertices should not increase");


    // Create the backward mapping and the new vertices
    IndexList backward_mapping(num_new_vertices, invalid<Index>());
    VertexArray vertices_new(num_new_vertices, mesh.get_dim());
    for (const auto io : range(num_old_vertices)) {
        const auto in = forward_mapping_no_invalid(io);
        backward_mapping[in] = io;
        vertices_new.row(in) = vertices.row(io);
    }
    // The mapping should be surjective.
    la_runtime_assert(
        std::find(backward_mapping.begin(), backward_mapping.end(), invalid<Index>()) ==
            backward_mapping.end(),
        "Forward mapping is not surjective");

    // Create new faces
    FacetArray facets_new(facets.rows(), facets.cols());
    for (auto i : range(facets.rows())) {
        facets_new(i, 0) = forward_mapping_no_invalid(facets(i, 0));
        facets_new(i, 1) = forward_mapping_no_invalid(facets(i, 1));
        facets_new(i, 2) = forward_mapping_no_invalid(facets(i, 2));
    }

    // Create new mesh
    auto mesh2 = create_mesh(std::move(vertices_new), std::move(facets_new));

    // Port attributes
    map_attributes(mesh, *mesh2, backward_mapping);

    return mesh2;
}

} // namespace legacy
} // namespace lagrange

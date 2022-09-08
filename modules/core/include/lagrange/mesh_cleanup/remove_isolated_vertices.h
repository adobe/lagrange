/*
 * Copyright 2017 Adobe. All rights reserved.
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

#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/attributes/map_attributes.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>

namespace lagrange {

template <typename MeshType>
std::unique_ptr<MeshType> remove_isolated_vertices(const MeshType& mesh)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    using Index = typename MeshType::Index;
    const Index dim = mesh.get_dim();
    const Index num_vertices = mesh.get_num_vertices();
    const Index num_facets = mesh.get_num_facets();
    const Index vertex_per_facet = mesh.get_vertex_per_facet();

    // Note: this is a forward mapping (for each element of mesh, index of new element)
    std::vector<Index> forward_vertex_map(num_vertices, invalid<Index>());

    const auto& vertices = mesh.get_vertices();
    auto facets = mesh.get_facets(); // copy, we modify this

    int count = 0;
    for (Index i = 0; i < num_facets; i++) {
        for (Index j = 0; j < vertex_per_facet; j++) {
            if (forward_vertex_map[facets(i, j)] == invalid<Index>()) {
                forward_vertex_map[facets(i, j)] = count;
                count++;
            }
            facets(i, j) = forward_vertex_map[facets(i, j)];
        }
    }

    const Index new_num_vertices = count;
    typename MeshType::VertexArray new_vertices(new_num_vertices, dim);
    for (Index i = 0; i < num_vertices; i++) {
        if (forward_vertex_map[i] == invalid<Index>()) continue;
        new_vertices.row(forward_vertex_map[i]) = vertices.row(i);
    }

    auto mesh2 = create_mesh(new_vertices, facets);

    // Port attributes
    map_attributes(mesh, *mesh2, invert_mapping(forward_vertex_map, new_num_vertices));

    return mesh2;
}
} // namespace lagrange

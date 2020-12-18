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

#include <exception>
#include <unordered_set>

#include <lagrange/Edge.h>
#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/chain_edges.h>
#include <lagrange/common.h>
#include <lagrange/get_opposite_edge.h>
#include <lagrange/utils/safe_cast.h>

namespace lagrange {

/**
 * Check if the input mesh is vertex manifold for all vertices.
 */
template <typename MeshType>
bool is_vertex_manifold(const MeshType& mesh)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    if (!mesh.is_connectivity_initialized()) {
        throw std::runtime_error("Connectivity needs to be initialized!");
    }
    if (mesh.get_vertex_per_facet() != 3) {
        throw std::runtime_error("Vertex manifold check is for triangle mesh only.");
    }

    using Index = typename MeshType::Index;
    using IndexList = typename MeshType::IndexList;
    using Edge = typename MeshType::Edge;

    const auto num_vertices = mesh.get_num_vertices();
    const auto& facets = mesh.get_facets();

    auto compute_euler_characteristic = [&facets](const IndexList& adj_facets) -> int {
        std::unordered_set<Index> vertices;
        EdgeSet<Index> edges;
        for (const auto fid : adj_facets) {
            Index v0{facets(fid, 0)};
            Index v1{facets(fid, 1)};
            Index v2{facets(fid, 2)};
            edges.insert({v0, v1});
            edges.insert({v1, v2});
            edges.insert({v2, v0});
            vertices.insert(v0);
            vertices.insert(v1);
            vertices.insert(v2);
        }

        // Euelr characteristic can be negative!
        return safe_cast<int>(vertices.size()) - safe_cast<int>(edges.size()) +
               safe_cast<int>(adj_facets.size());
    };

    for (Index i = 0; i < num_vertices; i++) {
        const auto& adj_facets = mesh.get_facets_adjacent_to_vertex(i);

        // A necessary but not sufficient check:
        // For each vertex, its one ring neighborhood should have Euler
        // characteristic 1 if it has disk topology.
        if (compute_euler_characteristic(adj_facets) != 1) {
            return false;
        } else {
            std::list<Edge> rim_edges;
            for (const auto fid : adj_facets) {
                rim_edges.push_back(get_opposite_edge(facets, fid, i));
            }
            const auto chains = chain_edges<Index>(rim_edges);
            if (chains.size() > 1) {
                return false;
            }
        }
    }
    return true;
}
} // namespace lagrange

/*
 * Copyright 2018 Adobe. All rights reserved.
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
#include <exception>
#include <limits>
#include <vector>

#include <lagrange/common.h>
#include <lagrange/Edge.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/utils/range.h>

namespace lagrange {

/**
 * Extract boundary loops.
 *
 * Returns an array of loops.  Each loop is an array of vertex indices.
 * Note that loop.front() == loop.back() if the loop is closed.
 *
 * Precondition: I am assuming the loops are simple (each vertex in a loop
 * is adjacent to 2 and only 2 edges).  Requiring the mesh to be manifold is
 * a sufficient but not necessary condition for the boundary loops to be
 * simple.
 */
template <typename MeshType>
std::vector<std::vector<typename MeshType::Index>> extract_boundary_loops(MeshType& mesh)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    using Index = typename MeshType::Index;
    const Index num_vertices = mesh.get_num_vertices();

    mesh.initialize_edge_data();
    std::vector<Index> boundary_next(num_vertices, invalid<Index>());

    const Index num_edges = mesh.get_num_edges();
    for (auto ei : range(num_edges)) {
        if (mesh.is_boundary_edge(ei)) {
            const auto edge = mesh.get_edge_vertices(ei);
            if (boundary_next[edge[0]] != invalid<Index>() && boundary_next[edge[0]] != edge[1]) {
                throw std::runtime_error("The boundary loops are not simple.");
            }
            boundary_next[edge[0]] = edge[1];
        }
    }

    std::vector<std::vector<Index>> bd_loops;
    bd_loops.reserve(4);
    for (Index i = 0; i < num_vertices; i++) {
        if (boundary_next[i] != invalid<Index>()) {
            bd_loops.emplace_back();
            auto& loop = bd_loops.back();

            Index curr_idx = i;
            loop.push_back(curr_idx);
            while (boundary_next[curr_idx] != invalid<Index>()) {
                loop.push_back(boundary_next[curr_idx]);
                boundary_next[curr_idx] = invalid<Index>();
                curr_idx = loop.back();
            }
            assert(!loop.empty());
            assert(loop.front() == loop.back());
        }
    }
    return bd_loops;
}
} // namespace lagrange

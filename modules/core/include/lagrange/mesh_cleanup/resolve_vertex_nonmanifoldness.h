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
#include <list>
#include <numeric>
#include <unordered_map>

#include <lagrange/Edge.h>
#include <lagrange/Logger.h>
#include <lagrange/Mesh.h>
#include <lagrange/attributes/map_attributes.h>
#include <lagrange/chain_edges.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/get_opposite_edge.h>

namespace lagrange {

/**
 * Remove nonmanifold vertices topologically by pulling disconnected 1-ring
 * neighborhood apart.
 *
 * Warning: This function assumes the input mesh contains **no** nonmanifold
 * edges or inconsistently oriented triangles.  If that is not the case,
 * consider using `lagrange::resolve_nonmanifoldness()` instead.
 */
template <typename MeshType>
std::unique_ptr<MeshType> resolve_vertex_nonmanifoldness(MeshType& mesh)
{
    if (!mesh.is_connectivity_initialized()) {
        mesh.initialize_connectivity();
    }
    if (mesh.get_vertex_per_facet() != 3) {
        throw std::runtime_error(
            "Resolve vertex nonmanifoldness is only implemented for triangle mesh");
    }

    using Index = typename MeshType::Index;
    using Edge = typename MeshType::Edge;
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;

    const auto dim = mesh.get_dim();
    const Index num_vertices = mesh.get_num_vertices();
    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();

    FacetArray out_facets(facets);
    Index vertex_count = mesh.get_num_vertices();

    // this is a backward vertex map, we initialize with iota as {0, 1, 2, 3, ...}
    // later we add elements gradually, since we don't know the final size in advance
    // This sounds really bad, but in practice there are not many non-manifold vertices,
    // and because of std::vector's memory allocation strategy, copying will typically
    // never happen, or at most once.
    std::vector<Index> backward_vertex_map(vertex_count);
    std::iota(backward_vertex_map.begin(), backward_vertex_map.end(), 0);

    for (Index i = 0; i < num_vertices; i++) {
        const auto& adj_facets = mesh.get_facets_adjacent_to_vertex(i);
        std::list<Edge> rim_edges;
        for (Index fid : adj_facets) {
            rim_edges.push_back(get_opposite_edge(facets, fid, i));
        }
        auto chains = chain_edges<Index>(rim_edges);
        if (chains.size() > 1) {
            std::unordered_map<Index, Index> comp_map;
            Index comp_count = 0;
            for (const auto& chain : chains) {
                for (const auto vid : chain) {
                    comp_map[vid] = comp_count;
                }
                comp_count++;
            }

            // Assign a new vertex for each additional rim loops.
            for (Index fid : adj_facets) {
                auto& f = facets.row(fid);
                for (Index j = 0; j < 3; j++) {
                    if (f[j] == i) {
                        const Index next = (j + 1) % 3;
                        const Index prev = (j + 2) % 3;
                        assert(comp_map.find(f[next]) != comp_map.end());
                        assert(comp_map.find(f[prev]) != comp_map.end());
                        if (comp_map[f[next]] != comp_map[f[prev]]) {
                            logger().trace("vertex: {}", i);
                            for (auto fi : adj_facets) {
                                logger().trace("{}", facets.row(fi));
                            }
                            throw std::runtime_error(
                                "Complex edge loop detected.  Vertex " + std::to_string(i) +
                                "'s one ring neighborhood must contain nonmanifold_edges!");
                        }
                        assert(comp_map[f[next]] == comp_map[f[prev]]);
                        const Index comp_id = comp_map[f[next]];
                        if (comp_id > 0) {
                            const Index new_vertex_index = vertex_count + comp_id - 1;
                            out_facets(fid, j) = new_vertex_index;
                            if (safe_cast<Index>(backward_vertex_map.size()) <= new_vertex_index) {
                                backward_vertex_map.resize(new_vertex_index + 1, INVALID<Index>());
                            }
                            backward_vertex_map[new_vertex_index] = i;
                        }
                        break;
                    }
                }
            }
            vertex_count += safe_cast<Index>(chains.size()) - 1;
        }
    }
    assert(out_facets.rows() == 0 || out_facets.maxCoeff() == vertex_count - 1);

    // all vertices between 0 and num_vertices are the same, so we can block-copy
    VertexArray out_vertices(vertex_count, dim);
    out_vertices.block(0, 0, num_vertices, dim) = vertices;
    for (Index i = num_vertices; i < vertex_count; i++) {
        out_vertices.row(i) = vertices.row(backward_vertex_map[i]);
    }

    auto out_mesh = create_mesh(std::move(out_vertices), std::move(out_facets));

    map_attributes(mesh, *out_mesh, backward_vertex_map);

    return out_mesh;
}
} // namespace lagrange

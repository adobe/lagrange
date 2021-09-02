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

#include <memory>
#include <numeric>
#include <set>
#include <unordered_map>
#include <vector>

#include <lagrange/Mesh.h>
#include <lagrange/attributes/map_attributes.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/mesh_cleanup/remove_duplicate_facets.h>
#include <lagrange/mesh_cleanup/remove_isolated_vertices.h>
#include <lagrange/mesh_cleanup/remove_topologically_degenerate_triangles.h>
#include <lagrange/mesh_cleanup/resolve_vertex_nonmanifoldness.h>


namespace lagrange {

/**
 * Resolve **all** non-manifold edges and vertices in the mesh.
 *
 * Arguments:
 *   mesh: The input mesh.
 *
 * Returns:
 *   A mesh same as the input mesh except non-manifold vertices and edges
 *   are pulled apart topologically.
 */
template <typename MeshType>
std::unique_ptr<MeshType> resolve_nonmanifoldness(MeshType& mesh)
{
    if (!mesh.is_connectivity_initialized()) {
        mesh.initialize_connectivity();
    }

    mesh.initialize_edge_data();

    using Index = typename MeshType::Index;
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;

    const Index num_vertices = mesh.get_num_vertices();
    const Index num_facets = mesh.get_num_facets();
    const Index vertex_per_facet = mesh.get_vertex_per_facet();
    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();


    /**
     * Return true iff Edge e is consistently oriented with the specified
     * facet.
     *
     * This method assumes e is a valid edge in the facet.
     */
    auto get_orientation = [&facets](const Index v0, const Index v1, const Index fid) -> bool {
        const auto& f = facets.row(fid);
        if (f[0] == v0 && f[1] == v1)
            return true;
        else if (f[1] == v0 && f[2] == v1)
            return true;
        else if (f[2] == v0 && f[0] == v1)
            return true;
        else
            return false;
    };

    /**
     * Return true iff facets around an edge are **inconsistently** oriented.
     * E.g. f0: * [0, 1, 2] and f1: [1, 2, 3], with e=[1, 2]
     *
     * @note: This method does not depend on the orientaiton of the edge e.
     *        This method also assumes the edge `ei` has exactly 2 adjacent
     *        facets.
     */
    auto is_inconsistently_oriented = [&mesh, &get_orientation](const Index ei) -> bool {
        const auto e = mesh.get_edge_vertices(ei);
        std::array<bool, 2> orientations;
        size_t count = 0;
        mesh.foreach_facets_around_edge(ei, [&](Index fid) {
            orientations[count] = get_orientation(e[0], e[1], fid);
            count++;
        });
        return orientations[0] == orientations[1];
    };

    /**
     * Return true iff f0 and f1 are consistantly oriented with respect to edge
     * ei.  This is **almost** the same as `is_inconsistently_oriented`.
     * With `f0` and `f1` specified, this version can work with non-manifold
     * edges.
     */
    auto is_inconsistently_oriented_wrt_facets =
        [&mesh, &get_orientation](const Index ei, const Index f0, const Index f1) {
            const auto e = mesh.get_edge_vertices(ei);
            return get_orientation(e[0], e[1], f0) == get_orientation(e[0], e[1], f1);
        };

    /**
     * Return true iff edge e has more than 2 incident facets or it has
     * exactly 2 incident facet but they are inconsistently oriented.
     */
    auto is_nonmanifold_edge = [&mesh, &is_inconsistently_oriented](const Index ei) {
        auto edge_valence = mesh.get_num_facets_around_edge(ei);
        if (edge_valence > 2) return true;
        if (edge_valence <= 1) return false;
        return is_inconsistently_oriented(ei);
    };

    // Flood fill color across manifold edges.  The color field split the
    // facets into locally manifold components.  Edges and vertices adjacent
    // to multiple colors will be split.
    constexpr Index BLANK = 0;
    std::vector<Index> colors(num_facets, BLANK);
    Index curr_color = 1;
    for (Index i = 0; i < num_facets; i++) {
        if (colors[i] != BLANK) continue;
        std::queue<Index> Q;
        Q.push(i);
        colors[i] = curr_color;
        while (!Q.empty()) {
            Index curr_fid = Q.front();
            Q.pop();
            for (Index j = 0; j < vertex_per_facet; j++) {
                Index ei = mesh.get_edge(curr_fid, j);
                if (is_nonmanifold_edge(ei)) continue;

                mesh.foreach_facets_around_edge(ei, [&](Index adj_fid) {
                    if (colors[adj_fid] == BLANK) {
                        colors[adj_fid] = curr_color;
                        Q.push(adj_fid);
                    }
                });
            }
        }
        curr_color++;
    }

    Index vertex_count = num_vertices;
    // Note:
    // The goal of vertex_map is to split the 1-ring neighborhood of a
    // non-manifold vertex based on the colors of its adjacent facets.  All
    // adjacent facets sharing the sanme color will share the same copy of
    // this vertex.
    //
    // To achieve this, I store a color map for each non-manifold vertex,
    // where vertex_map maps a non-manifold vertex to its color map.
    // A color map maps specific color to the vertex index after the split.
    std::unordered_map<Index, std::unordered_map<Index, Index>> vertex_map;
    // Split non-manifold edges.
    for (Index i = 0; i < mesh.get_num_edges(); ++i) {
        const auto e = mesh.get_edge_vertices(i);

        if (!is_nonmanifold_edge(i)) continue;
        auto itr0 = vertex_map.find(e[0]);
        auto itr1 = vertex_map.find(e[1]);

        if (itr0 == vertex_map.end()) {
            itr0 = vertex_map.insert({e[0], std::unordered_map<Index, Index>()}).first;
        }
        if (itr1 == vertex_map.end()) {
            itr1 = vertex_map.insert({e[1], std::unordered_map<Index, Index>()}).first;
        }

        auto& color_map_0 = itr0->second;
        auto& color_map_1 = itr1->second;

        std::unordered_map<Index, std::list<Index>> color_count;
        mesh.foreach_facets_around_edge(i, [&](Index fid) {
            const auto c = colors[fid];

            auto c_itr = color_count.find(c);
            if (c_itr == color_count.end()) {
                color_count.insert({c, {fid}});
            } else {
                c_itr->second.push_back(fid);
            }

            auto c_itr0 = color_map_0.find(c);
            if (c_itr0 == color_map_0.end()) {
                if (color_map_0.size() == 0) {
                    color_map_0.insert({c, e[0]});
                } else {
                    color_map_0.insert({c, vertex_count});
                    vertex_count++;
                }
            }

            auto c_itr1 = color_map_1.find(c);
            if (c_itr1 == color_map_1.end()) {
                if (color_map_1.size() == 0) {
                    color_map_1.insert({c, e[1]});
                } else {
                    color_map_1.insert({c, vertex_count});
                    vertex_count++;
                }
            }
        });

        for (const auto& entry : color_count) {
            // Corner case 1:
            // Exact two facets share the same color around this edge, but
            // they are inconsistently oriented.  Thus, they needs to be
            // detacted.
            const bool inconsistent_edge =
                (entry.second.size() == 2) &&
                is_inconsistently_oriented_wrt_facets(i, entry.second.front(), entry.second.back());

            // Corner case 2:
            // Some facets around this non-manifold edge are connected via
            // a chain of manifold edges.  Thus, they have the same color.
            // To resolve this, I am detacching all facets of this color
            // adjacent to this edge.
            const bool single_comp_nonmanifoldness = entry.second.size() > 2;

            if (single_comp_nonmanifoldness || inconsistent_edge) {
                // Each facet will be reconnect to a newly created edge.
                // This solution is not ideal, but works.
                for (const auto fid : entry.second) {
                    colors[fid] = curr_color;
                    color_map_0.insert({curr_color, vertex_count});
                    vertex_count++;
                    color_map_1.insert({curr_color, vertex_count});
                    vertex_count++;
                    curr_color++;
                }
            }
        }
    }

    // Split non-manifold vertices
    for (Index i = 0; i < num_vertices; i++) {
        const auto& adj_facets = mesh.get_facets_adjacent_to_vertex(i);
        std::set<Index> adj_colors;
        for (auto adj_f : adj_facets) {
            adj_colors.insert(colors[adj_f]);
        }
        if (adj_colors.size() <= 1) continue;

        auto itr = vertex_map.find(i);
        if (itr == vertex_map.end()) {
            vertex_map.insert({i, {}});
        }

        auto& vi_map = vertex_map[i];
        for (auto c : adj_colors) {
            auto c_itr = vi_map.find(c);
            if (c_itr == vi_map.end()) {
                if (vi_map.size() == 0) {
                    vi_map.insert({c, i});
                } else {
                    vi_map.insert({c, vertex_count});
                    vertex_count++;
                }
            }
        }
    }

    VertexArray manifold_vertices(vertex_count, vertices.cols());
    manifold_vertices.topRows(num_vertices) = vertices;

    std::vector<Index> backward_vertex_map(vertex_count, 0);
    std::iota(backward_vertex_map.begin(), backward_vertex_map.begin() + num_vertices, 0);

    for (const auto& itr : vertex_map) {
        Index vid = itr.first;
        for (const auto& itr2 : itr.second) {
            manifold_vertices.row(itr2.second) = vertices.row(vid);
            backward_vertex_map[itr2.second] = vid;
        }
    }

    FacetArray manifold_facets = facets;
    for (Index i = 0; i < num_facets; i++) {
        const Index c = colors[i];
        for (Index j = 0; j < vertex_per_facet; j++) {
            const auto& itr = vertex_map.find(manifold_facets(i, j));
            if (itr != vertex_map.end()) {
                manifold_facets(i, j) = itr->second[c];
            }
        }
    }

    auto out_mesh = create_mesh(std::move(manifold_vertices), std::move(manifold_facets));

    map_attributes(mesh, *out_mesh, backward_vertex_map);

    out_mesh->initialize_connectivity();
    out_mesh->initialize_edge_data();

    out_mesh = resolve_vertex_nonmanifoldness(*out_mesh);
    out_mesh = remove_topologically_degenerate_triangles(*out_mesh);
    out_mesh = remove_duplicate_facets(*out_mesh);
    out_mesh = remove_isolated_vertices(*out_mesh);
    return out_mesh;
}
} // namespace lagrange

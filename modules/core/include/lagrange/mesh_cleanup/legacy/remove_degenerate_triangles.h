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

#include <algorithm>
#include <set>
#include <vector>

#include <lagrange/Edge.h>
#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/attributes/map_corner_attributes.h>
#include <lagrange/common.h>
#include <lagrange/legacy/inline.h>
#include <lagrange/mesh_cleanup/detect_degenerate_triangles.h>
#include <lagrange/mesh_cleanup/remove_short_edges.h>
#include <lagrange/mesh_cleanup/split_triangle.h>
#include <lagrange/utils/safe_cast.h>

namespace lagrange {
LAGRANGE_LEGACY_INLINE
namespace legacy {

/**
 * Remove all exactly degenerate triangles.
 *
 * Arguments:
 *     input_mesh
 *
 * Returns:
 *     output_mesh without any exactly degenerate triangles.
 *
 * All vertex and facet attributes are mapped over.
 */

template <typename MeshType>
std::unique_ptr<MeshType> remove_degenerate_triangles(const MeshType& mesh)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    using Index = typename MeshType::Index;
    using Facet = typename Eigen::Matrix<Index, 3, 1>;
    using Edge = typename MeshType::Edge;

    lagrange::logger().trace("[remove_degenerate_triangles]");
    auto out_mesh = remove_short_edges(mesh);

    detect_degenerate_triangles(*out_mesh);
    la_runtime_assert(out_mesh->has_facet_attribute("is_degenerate"));
    const auto& is_degenerate = out_mesh->get_facet_attribute("is_degenerate");
    const Index num_degenerate_faces = safe_cast<Index>(is_degenerate.sum());
    if (num_degenerate_faces == 0) {
        return out_mesh;
    }

    const Index dim = out_mesh->get_dim();
    const auto& vertices = out_mesh->get_vertices();
    const Index num_vertices = out_mesh->get_num_vertices();
    const Index num_facets = out_mesh->get_num_facets();
    const Index vertex_per_facet = out_mesh->get_vertex_per_facet();
    la_runtime_assert(vertex_per_facet == 3);
    const auto& facets = out_mesh->get_facets();
    la_runtime_assert(facets.minCoeff() >= 0);
    la_runtime_assert(facets.maxCoeff() < num_vertices);

    auto on_edge = [&vertices, dim](const Edge& edge, const Index pid) {
        for (Index i = 0; i < dim; i++) {
            double ep1 = vertices(edge[0], i);
            double ep2 = vertices(edge[1], i);
            double p = vertices(pid, i);
            if (ep1 > p && ep2 < p)
                return true;
            else if (ep1 < p && ep2 > p)
                return true;
            else if (ep1 == p && ep2 == p)
                continue;
            else
                return false;
        }
        // edge is degenrated with both end points equals to pid.
        throw std::runtime_error("Edge must be degenerated.");
    };

    // Compute edge facet adjacency for triangles wihtin 1-ring
    // neighborhood of degenerate triangles.
    std::unordered_set<Index> active_vertices;
    std::unordered_set<Index> active_facets;
    for (Index i = 0; i < num_facets; ++i) {
        if (is_degenerate(i, 0)) {
            active_facets.insert(i);
            active_vertices.insert(facets(i, 0));
            active_vertices.insert(facets(i, 1));
            active_vertices.insert(facets(i, 2));
        }
    }
    // consolidate region. activate all facets adjacent to an active vertex.
    // note: might want to move this inside compute_edge_facet_map_in_active_facets
    for (Index i = 0; i < num_facets; ++i) {
        if (active_facets.find(i) == active_facets.end()) { // was it active already?
            for (Index j = 0; j < vertex_per_facet; ++j) {
                if (active_vertices.find(facets(i, j)) != active_vertices.end()) {
                    active_facets.insert(i);
                }
            }
        }
    }
    auto edge_facet_map = compute_edge_facet_map_in_active_facets(*out_mesh, active_facets);

    std::vector<bool> visited(num_facets, false);
    std::function<void(std::set<Index>&, EdgeSet<Index>&, const Index)> get_collinear_pts;
    get_collinear_pts =
        [&edge_facet_map, &facets, &visited, vertex_per_facet, &get_collinear_pts, &is_degenerate](
            std::set<Index>& collinear_pts,
            EdgeSet<Index>& involved_edges,
            const Index fid) {
            if (visited[fid]) return;
            visited[fid] = true;
            if (!is_degenerate(fid, 0)) return;

            // Base case.
            for (Index i = 0; i < vertex_per_facet; i++) {
                Index v = facets(fid, i);
                collinear_pts.insert(v);
                Edge e{{facets(fid, i), facets(fid, (i + 1) % vertex_per_facet)}};
                involved_edges.insert(e);
            }

            // Recurse.
            for (Index i = 0; i < vertex_per_facet; i++) {
                Edge edge{{facets(fid, i), facets(fid, (i + 1) % vertex_per_facet)}};
                const auto itr = edge_facet_map.find(edge);
                la_runtime_assert(itr != edge_facet_map.end());
                for (const auto next_fid : itr->second) {
                    get_collinear_pts(collinear_pts, involved_edges, next_fid);
                }
            }
        };

    using SplittingPts = std::vector<Index>;
    EdgeMap<Index, SplittingPts> splitting_points;
    for (const auto& item : edge_facet_map) {
        // const auto& edge = item.first;
        const auto& adj_facets = item.second;
        std::set<Index> collinear_pts;
        EdgeSet<Index> involved_edges;
        for (const auto& fid : adj_facets) {
            if (!is_degenerate(fid, 0)) continue;
            get_collinear_pts(collinear_pts, involved_edges, fid);
            break;
        }

        for (const auto& edge : involved_edges) {
            SplittingPts pts;
            for (const auto vid : collinear_pts) {
                if (on_edge(edge, vid)) {
                    pts.push_back(vid);
                }
            }
            splitting_points[edge] = pts;
        }
    }

    const auto index_comp = [&vertices, dim](const Index i, const Index j) {
        for (Index d = 0; d < dim; d++) {
            if (vertices(i, d) == vertices(j, d))
                continue;
            else
                return vertices(i, d) < vertices(j, d);
        }
        return false;
    };

    for (auto& item : splitting_points) {
        std::sort(item.second.begin(), item.second.end(), index_comp);
    }

    std::vector<Facet> splitted_facets;
    std::vector<Index> facet_map;
    // const auto& active_facets = edge_facet_map.active_facets();
    for (Index i = 0; i < num_facets; i++) {
        if (active_facets.find(i) == active_facets.end()) { // !active_facets[i]) {
            splitted_facets.emplace_back(facets.row(i));
            facet_map.push_back(i);
            continue;
        } else if (is_degenerate(i)) {
            continue;
        }

        std::vector<Index> chain;
        chain.reserve(8);
        Index v[3];
        const Facet f = facets.row(i);
        for (Index j = 0; j < 3; j++) {
            Edge e{{f[j], f[(j + 1) % 3]}};
            v[j] = safe_cast<Index>(chain.size());
            chain.push_back(f[j]);
            const auto itr = splitting_points.find(e);
            if (itr != splitting_points.end()) {
                const auto& pts = itr->second;
                if (index_comp(e[0], e[1])) {
                    std::copy(pts.begin(), pts.end(), std::back_inserter(chain));
                } else {
                    std::copy(pts.rbegin(), pts.rend(), std::back_inserter(chain));
                }
            }
        }

        if (chain.size() == 3) {
            splitted_facets.emplace_back(facets.row(i));
            facet_map.push_back(i);
        } else {
            auto new_facets = split_triangle(vertices, chain, v[0], v[1], v[2]);
            std::move(new_facets.begin(), new_facets.end(), std::back_inserter(splitted_facets));
            facet_map.insert(facet_map.end(), new_facets.size(), i);
        }
    }

    const auto num_out_facets = splitted_facets.size();
    typename MeshType::FacetArray out_facets(num_out_facets, 3);
    for (auto i : range(num_out_facets)) {
        out_facets.row(i) = splitted_facets[i];
    }
    la_runtime_assert(num_out_facets == 0 || out_facets.minCoeff() >= 0);
    la_runtime_assert(num_out_facets == 0 || out_facets.maxCoeff() < num_vertices);

    auto out_mesh2 = create_mesh(vertices, out_facets);

    // Port attributes
    map_attributes(*out_mesh, *out_mesh2, {}, facet_map);

    return remove_short_edges(*out_mesh2);
}

} // namespace legacy
} // namespace lagrange

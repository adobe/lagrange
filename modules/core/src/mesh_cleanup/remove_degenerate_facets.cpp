/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/mesh_cleanup/detect_degenerate_facets.h>
#include <lagrange/mesh_cleanup/remove_degenerate_facets.h>
#include <lagrange/mesh_cleanup/remove_duplicate_vertices.h>
#include <lagrange/mesh_cleanup/remove_topologically_degenerate_facets.h>
#include <lagrange/utils/SmallVector.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/internal/split_edges.h>
#include <lagrange/internal/split_triangle.h>
#include <lagrange/views.h>


#include <algorithm>
#include <vector>

namespace lagrange {

template <typename Scalar, typename Index>
void remove_degenerate_facets(SurfaceMesh<Scalar, Index>& mesh)
{
    using SVector = SmallVector<Index, 256>;

    la_runtime_assert(mesh.is_triangle_mesh());
    remove_duplicate_vertices(mesh);
    remove_topologically_degenerate_facets(mesh);
    auto degenerate_facets = detect_degenerate_facets(mesh);
    if (degenerate_facets.empty()) return;

    mesh.initialize_edges();
    const Index num_facets = mesh.get_num_facets();
    const Index num_edges = mesh.get_num_edges();

    std::vector<bool> is_degenerate(num_facets, false);
    std::vector<bool> visited(num_facets, false);
    for (Index f : degenerate_facets) {
        is_degenerate[f] = true;
    }

    // Recursively search for connected degenerate facets that are collinear with the input facet.
    auto get_neighboring_collinear_facets =
        [&](Index fid, SVector& collinear_facets, auto&& recurse_fn) {
            la_debug_assert(is_degenerate[fid]);
            if (visited[fid]) return;

            visited[fid] = true;
            collinear_facets.push_back(fid);
            size_t curr_size = collinear_facets.size();

            auto facet_size = mesh.get_facet_size(fid);
            for (Index i = 0; i < facet_size; i++) {
                Index eid = mesh.get_edge(fid, i);
                mesh.foreach_facet_around_edge(eid, [&](Index fid2) {
                    if (visited[fid2]) return;
                    if (!is_degenerate[fid2]) return;

                    visited[fid2] = true;
                    collinear_facets.push_back(fid2);
                });
            }

            // Recursively search for added collinear facets
            for (size_t i = curr_size; i < collinear_facets.size(); i++) {
                recurse_fn(collinear_facets[i], collinear_facets, recurse_fn);
            }
        };

    auto get_involved_vertices = [&](const SVector& collinear_facets, SVector& involved_vertices) {
        involved_vertices.clear();
        for (Index fid : collinear_facets) {
            auto facet_size = mesh.get_facet_size(fid);
            for (Index i = 0; i < facet_size; i++) {
                Index vid = mesh.get_facet_vertex(fid, i);
                involved_vertices.push_back(vid);
            }
        }
        std::sort(involved_vertices.begin(), involved_vertices.end());
        auto end = std::unique(involved_vertices.begin(), involved_vertices.end());
        involved_vertices.erase(end, involved_vertices.end());
    };

    // Check if a vertex collinear with an edge is actually on the edge.
    // Note vertices that are on the end points of the edge are not considered as on the edge.
    auto on_edge = [&](Index eid, Index vid) {
        const auto dim = mesh.get_dimension();
        const auto p = mesh.get_position(vid);
        const auto [v0, v1] = mesh.get_edge_vertices(eid);
        const auto p0 = mesh.get_position(v0);
        const auto p1 = mesh.get_position(v1);
        for (Index i = 0; i < dim; i++) {
            if (p0[i] > p[i] && p1[i] < p[i])
                return true;
            else if (p0[i] < p[i] && p1[i] > p[i])
                return true;
            else if (p0[i] == p[i] && p1[i] == p[i])
                continue;
            else
                return false;
        }
        // Edge is degenerate, and vid is on the degenerate edge.
        la_runtime_assert(false, "Degenerate edge detected");
        return false;
    };

    // Gather split points on the edge. The split vertices are ordered along the edge.
    auto gather_split_points =
        [&](Index eid, const SVector& collinear_vertices, std::vector<Index>& split_points) {
            size_t begin_size = split_points.size();
            for (Index vid : collinear_vertices) {
                if (on_edge(eid, vid)) {
                    split_points.push_back(vid);
                }
            }

            const auto dim = mesh.get_dimension();
            const auto [v0, v1] = mesh.get_edge_vertices(eid);
            const auto p0 = mesh.get_position(v0);
            const auto p1 = mesh.get_position(v1);

            auto comp = [&](Index vi, Index vj) {
                const auto pi = mesh.get_position(vi);
                const auto pj = mesh.get_position(vj);
                for (Index i = 0; i < dim; i++) {
                    if (p0[i] == p1[i]) continue;
                    if (p0[i] < p1[i])
                        return pi[i] < pj[i];
                    else
                        return pi[i] > pj[i];
                }
                // p0 == p1
                la_runtime_assert(false, "Degenerate edge detected");
            };
            std::sort(split_points.begin() + begin_size, split_points.end(), comp);
        };

    std::vector<Index> edge_split_points;
    std::vector<std::array<size_t, 2>> edge_split_offsets(num_edges, {0, 0});
    edge_split_points.reserve(128);

    // Populate edge_split_points
    {
        SVector collinear_facets;
        SVector collinear_vertices;

        for (Index fid : degenerate_facets) {
            if (visited[fid]) continue;
            collinear_facets.clear();
            get_neighboring_collinear_facets(
                fid,
                collinear_facets,
                get_neighboring_collinear_facets);
            get_involved_vertices(collinear_facets, collinear_vertices);

            for (Index fid2 : collinear_facets) {
                Index facet_size = mesh.get_facet_size(fid2);
                for (Index i = 0; i < facet_size; i++) {
                    Index eid = mesh.get_edge(fid2, i);
                    auto& offset = edge_split_offsets[eid];
                    if (offset[0] == offset[1]) {
                        offset[0] = edge_split_points.size();
                        gather_split_points(eid, collinear_vertices, edge_split_points);
                        offset[1] = edge_split_points.size();
                    }
                }
            }
        }
    }

    auto facets_to_remove = internal::split_edges(
        mesh,
        function_ref<span<Index>(Index)>([&](Index eid) {
            const auto& offset = edge_split_offsets[eid];
            return span<Index>(edge_split_points.data() + offset[0], offset[1] - offset[0]);
        }),
        function_ref<bool(Index)>([&](Index fid) { return !is_degenerate[fid]; }));

    // Mark facets to remove as degenerate and remove both degenerate and out-of-date facets.
    for (auto fid : facets_to_remove) {
        is_degenerate[fid] = true;
    }
    mesh.remove_facets(function_ref<bool(Index)>(
        [&](Index fid) { return fid < is_degenerate.size() && is_degenerate[fid]; }));
}

#define LA_X_remove_degenerate_facets(_, Scalar, Index) \
    template LA_CORE_API void remove_degenerate_facets<Scalar, Index>(SurfaceMesh<Scalar, Index>&);
LA_SURFACE_MESH_X(remove_degenerate_facets, 0)

} // namespace lagrange

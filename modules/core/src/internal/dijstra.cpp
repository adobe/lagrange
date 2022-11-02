/*
 * Copyright 2022 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/internal/dijkstra.h>

#include <limits>
#include <queue>
#include <vector>

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/utils/assert.h>

namespace lagrange::internal {

template <typename Scalar, typename Index>
void dijkstra(
    SurfaceMesh<Scalar, Index>& mesh,
    span<const Index> seed_vertices,
    span<const Scalar> seed_vertex_dist,
    Scalar radius,
    const function_ref<Scalar(Index, Index)>& dist,
    const function_ref<bool(Index, Scalar)>& process)
{
    if (radius <= 0) {
        radius = std::numeric_limits<Scalar>::max();
    }

    mesh.initialize_edges();

    const auto num_vertices = mesh.get_num_vertices();
    const auto num_edges = mesh.get_num_edges();

    using Entry = std::pair<Scalar, Index>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> Q;
    std::vector<bool> visited(num_vertices, false);

    size_t num_seeds = seed_vertices.size();
    la_runtime_assert(num_seeds == seed_vertex_dist.size());
    for (size_t i = 0; i < num_seeds; i++) {
        la_runtime_assert(seed_vertices[i] < num_vertices);
        Q.push({seed_vertex_dist[i], seed_vertices[i]});
    }

    std::vector<bool> visited_edges(num_edges, false);
    std::vector<Index> edge_indices;
    edge_indices.reserve(16);
    while (!Q.empty()) {
        Entry entry = Q.top();
        Q.pop();

        Index vi = entry.second;
        Scalar di = entry.first;

        if (visited[vi]) continue;

        bool done = process(vi, di);
        if (done) break;
        visited[vi] = true;

        edge_indices.clear();
        mesh.foreach_edge_around_vertex_with_duplicates(vi, [&](Index ei) {
            if (visited_edges[ei]) return;
            visited_edges[ei] = true;
            edge_indices.push_back(ei);

            auto e = mesh.get_edge_vertices(ei);
            Index vj = (e[0] == vi) ? e[1] : e[0];
            Scalar dj = di + dist(vi, vj);
            if (dj < radius) {
                Q.push({dj, vj});
            }
        });

        for (auto ei : edge_indices) {
            visited_edges[ei] = false;
        }
    }
}

#define LA_X_dijkstra(_, Scalar, Index)            \
    template void dijkstra<Scalar, Index>(         \
        SurfaceMesh<Scalar, Index>&,               \
        span<const Index>,                         \
        span<const Scalar>,                        \
        Scalar,                                    \
        const function_ref<Scalar(Index, Index)>&, \
        const function_ref<bool(Index, Scalar)>&);

LA_SURFACE_MESH_X(dijkstra, 0)

} // namespace lagrange::internal

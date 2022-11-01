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

#pragma once

#include <lagrange/Mesh.h>
#include <lagrange/legacy/inline.h>
#include <lagrange/utils/assert.h>

#include <array>
#include <queue>
#include <vector>

namespace lagrange {
namespace internal {
LAGRANGE_LEGACY_INLINE
namespace legacy {

/**
 * Traverse the mesh based on Dijkstra's algorithm with customized distance
 * metric and process functions.
 *
 * @param[in] mesh              The input mesh.
 * @param[in] seed_vertices     Seed vertices.
 * @param[in] seed_vertex_dist  Initial distance to seed vertices.
 *                              the seed triangle.
 * @param[in] radius            The radius of the search.
 * @param[in] dist              The distance function.  e.g. d = dist(v0, v1)
 * @param[in] process           Call back function to process each new vertex reached.
 *                              Its return value indicates whether the search is done.
 *                              e.g. done = call_back(vid, v_dist);
 */
template <typename MeshType, typename DistFunc, typename ProcessFunc>
void dijkstra(
    const MeshType& mesh,
    const std::vector<typename MeshType::Index>& seed_vertices,
    const std::vector<typename MeshType::Scalar>& seed_vertex_dist,
    typename MeshType::Scalar radius,
    const DistFunc& dist,
    const ProcessFunc& process)
{
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;

    la_runtime_assert(
        mesh.get_vertex_per_facet() == 3,
        "Only triangle meshes are supported for now.");
    la_runtime_assert(
        mesh.is_connectivity_initialized(),
        "BSF requires mesh connectivity to be initialized.");

    if (radius <= 0.0) {
        radius = std::numeric_limits<Scalar>::max();
    }

    const Index num_vertices = mesh.get_num_vertices();

    using Entry = std::pair<Index, Scalar>;
    auto comp = [](const Entry& e1, const Entry& e2) { return e1.second > e2.second; };
    std::priority_queue<Entry, std::vector<Entry>, decltype(comp)> Q(comp);
    std::vector<bool> visited(num_vertices, false);

    size_t num_seeds = seed_vertices.size();
    la_runtime_assert(
        num_seeds == seed_vertex_dist.size(),
        "Inconsistent number of seed distances.");
    for (size_t i = 0; i < num_seeds; i++) {
        la_runtime_assert(seed_vertices[i] < num_vertices);
        Q.push({seed_vertices[i], seed_vertex_dist[i]});
    }

    while (!Q.empty()) {
        Entry entry = Q.top();
        Q.pop();

        Index vi = entry.first;
        Scalar di = entry.second;

        if (visited[vi]) continue;

        bool done = process(vi, di);
        if (done) break;
        visited[vi] = true;

        const auto& adj_vertices = mesh.get_vertices_adjacent_to_vertex(entry.first);
        for (const auto& vj : adj_vertices) {
            Scalar dj = di + dist(vi, vj);
            if (dj < radius) {
                Q.push({vj, dj});
            }
        }
    }
}

} // namespace legacy
} // namespace internal
} // namespace lagrange

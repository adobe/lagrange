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

#pragma once

#include <lagrange/utils/assert.h>
#include <lagrange/utils/timing.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_sort.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
#include <numeric>

namespace lagrange::internal {

template <typename Index>
struct UnorientedEdge
{
    Index v1;
    Index v2;
    Index id;

    UnorientedEdge(Index x, Index y, Index c)
        : v1(std::min(x, y))
        , v2(std::max(x, y))
        , id(c)
    {}

    auto key() const { return std::make_pair(v1, v2); }

    bool operator<(const UnorientedEdge& e) const { return key() < e.key(); }

    bool operator!=(const UnorientedEdge& e) const { return key() != e.key(); }
};

///
/// Sort an array of edges using a parallel bucket sort.
///
/// @todo       Maybe we can implement a local cache system for SurfaceMesh<> to reuse tmp buffers.
///             Maybe this would make the add_vertex/add_facet functions efficient enough that we do
///             not need to allocate them all at once in our `triangulate_polygonal_facets`
///             function.
///
/// @param[in]  num_edges             Number of edges to sort.
/// @param[in]  num_vertices          Number of vertices in the mesh.
/// @param[in]  get_edge              Callback to retrieve the n-th edge endpoints. Must be safe to
///                                   call from multiple threads.
/// @param[in]  vertex_to_first_edge  Optional buffer of size num_vertices + 1 to avoid internal
///                                   allocations on repeated uses.
///
/// @tparam     Index                 Edge index type.
/// @tparam     Func                  Callback function to retrieve edge endpoints.
///
/// @return     A vector of sorted edge indices. Edges with repeated endpoints will be continuous in
///             the sorted array.
///
template <typename Index, typename Func>
std::vector<Index> fast_edge_sort(
    Index num_edges,
    Index num_vertices,
    Func get_edge,
    span<Index> vertex_to_first_edge = {})
{
    std::vector<Index> local_buffer;
    if (vertex_to_first_edge.empty()) {
        local_buffer.assign(num_vertices + 1, 0);
        vertex_to_first_edge = local_buffer;
    } else {
        std::fill(vertex_to_first_edge.begin(), vertex_to_first_edge.end(), Index(0));
    }
    la_runtime_assert(vertex_to_first_edge.size() == static_cast<size_t>(num_vertices) + 1);
    // Count number of edges starting at each vertex
    for (Index e = 0; e < num_edges; ++e) {
        std::array<Index, 2> v = get_edge(e);
        if (v[0] > v[1]) {
            std::swap(v[0], v[1]);
        }
        vertex_to_first_edge[v[0] + 1]++;
    }
    // Prefix sum to compute actual offsets
    std::partial_sum(
        vertex_to_first_edge.begin(),
        vertex_to_first_edge.end(),
        vertex_to_first_edge.begin());
    la_runtime_assert(vertex_to_first_edge.back() == num_edges);
    // Bucket each edge id to its respective starting vertex
    std::vector<Index> edge_ids(num_edges);
    for (Index e = 0; e < num_edges; ++e) {
        std::array<Index, 2> v = get_edge(e);
        if (v[0] > v[1]) {
            std::swap(v[0], v[1]);
        }
        edge_ids[vertex_to_first_edge[v[0]]++] = e;
    }
    // Shift back the offset buffer 'vertex_to_first_edge' (can use std::shift_right in C++20 :p)
    std::rotate(
        vertex_to_first_edge.rbegin(),
        vertex_to_first_edge.rbegin() + 1,
        vertex_to_first_edge.rend());
    vertex_to_first_edge.front() = 0;
    // Sort each bucket in parallel
    tbb::parallel_for(Index(0), num_vertices, [&](Index v) {
        tbb::parallel_sort(
            edge_ids.begin() + vertex_to_first_edge[v],
            edge_ids.begin() + vertex_to_first_edge[v + 1],
            [&](Index ei, Index ej) {
                auto vi = get_edge(ei);
                auto vj = get_edge(ej);
                if (vi[0] > vi[1]) std::swap(vi[0], vi[1]);
                if (vj[0] > vj[1]) std::swap(vj[0], vj[1]);
                return vi < vj;
            });
    });
    return edge_ids;
}

} // namespace lagrange::internal

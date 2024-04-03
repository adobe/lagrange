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
#include <algorithm>
#include <numeric>
#include <vector>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_vertex_vertex_adjacency.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/range.h>

namespace lagrange {

template <typename Scalar, typename Index>
AdjacencyList<Index> compute_vertex_vertex_adjacency(SurfaceMesh<Scalar, Index>& mesh)
{
    const auto num_vertices = mesh.get_num_vertices();
    const auto num_facets = mesh.get_num_facets();

    using ValueArray = typename AdjacencyList<Index>::ValueArray;
    using IndexArray = typename AdjacencyList<Index>::IndexArray;

    ValueArray adjacency_data;
    IndexArray adjacency_index(num_vertices + 1, 0);

    // Estimate max size.
    for (auto fi : range(num_facets)) {
        const Index c_begin = mesh.get_facet_corner_begin(fi);
        const Index c_end = mesh.get_facet_corner_end(fi);

        for (Index ci = c_begin; ci < c_end; ci++) {
            Index c_next = (ci + 1 == c_end) ? c_begin : ci + 1;
            Index v_curr = mesh.get_corner_vertex(ci);
            Index v_next = mesh.get_corner_vertex(c_next);
            adjacency_index[v_curr]++;
            adjacency_index[v_next]++;
        }
    }

    std::rotate(adjacency_index.rbegin(), adjacency_index.rbegin() + 1, adjacency_index.rend());
    std::partial_sum(adjacency_index.begin(), adjacency_index.end(), adjacency_index.begin());
    const Index total_size = static_cast<Index>(adjacency_index.back());
    adjacency_data.resize(total_size);

    // Gather adjacency data with duplicates. Note: We could do this loop in parallel by using a
    // vector of atomic counters for `adjacency_index`
    for (Index fi = 0; fi < num_facets; ++fi) {
        const Index c_begin = mesh.get_facet_corner_begin(fi);
        const Index c_end = mesh.get_facet_corner_end(fi);

        for (Index ci = c_begin; ci < c_end; ci++) {
            Index c_next = (ci + 1 == c_end) ? c_begin : ci + 1;
            Index v_curr = mesh.get_corner_vertex(ci);
            Index v_next = mesh.get_corner_vertex(c_next);

            adjacency_data[adjacency_index[v_curr]++] = v_next;
            adjacency_data[adjacency_index[v_next]++] = v_curr;
        }
    };

    // Remove duplicate data.
    tbb::parallel_for(Index(0), num_vertices, [&](Index vi) {
        auto itr_begin = std::next(adjacency_data.begin(), vi == 0 ? 0 : adjacency_index[vi - 1]);
        auto itr_end = std::next(adjacency_data.begin(), adjacency_index[vi]);
        std::sort(itr_begin, itr_end);
        auto new_itr_end = std::unique(itr_begin, itr_end);
        std::fill(new_itr_end, itr_end, invalid<Index>());
    });

    // Condense adjacency data.
    size_t count = 0;
    size_t start_idx = 0;
    for (auto vi : range(num_vertices)) {
        size_t end_idx = adjacency_index[vi];
        for (size_t i = start_idx; i < end_idx; i++) {
            if (adjacency_data[i] != invalid<Index>()) {
                adjacency_data[count++] = adjacency_data[i];
            } else {
                break;
            }
        }
        adjacency_index[vi] = count;
        start_idx = end_idx;
    }
    adjacency_data.resize(count);
    adjacency_data.shrink_to_fit();
    std::rotate(adjacency_index.rbegin(), adjacency_index.rbegin() + 1, adjacency_index.rend());
    adjacency_index.front() = 0;

    AdjacencyList<Index> adjacency_list(std::move(adjacency_data), std::move(adjacency_index));
    return adjacency_list;
}

#define LA_X_compute_vertex_vertex_adjacency(_, Scalar, Index) \
    template LA_CORE_API AdjacencyList<Index> compute_vertex_vertex_adjacency(SurfaceMesh<Scalar, Index>&);
LA_SURFACE_MESH_X(compute_vertex_vertex_adjacency, 0)

} // namespace lagrange

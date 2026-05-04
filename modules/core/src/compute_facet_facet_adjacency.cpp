/*
 * Copyright 2026 Adobe. All rights reserved.
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
#include <lagrange/compute_facet_facet_adjacency.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <numeric>
#include <vector>

namespace lagrange {

template <typename Scalar, typename Index>
AdjacencyList<Index> compute_facet_facet_adjacency(SurfaceMesh<Scalar, Index>& mesh)
{
    if (!mesh.has_edges()) {
        mesh.initialize_edges();
    }

    const Index num_facets = mesh.get_num_facets();

    using ValueArray = typename AdjacencyList<Index>::ValueArray;
    using IndexArray = typename AdjacencyList<Index>::IndexArray;

    // Count neighbors per facet using foreach_facet_around_facet, which already
    // skips self and handles non-manifold edges (complete clique).
    IndexArray adjacency_index(num_facets + 1, 0);
    tbb::parallel_for(Index(0), num_facets, [&](Index f) {
        mesh.foreach_facet_around_facet(f, [&](Index) { adjacency_index[f + 1]++; });
    });

    // Prefix sum to get offsets.
    std::partial_sum(adjacency_index.begin(), adjacency_index.end(), adjacency_index.begin());

    // Fill adjacency data.
    ValueArray adjacency_data(adjacency_index.back());
    tbb::parallel_for(Index(0), num_facets, [&](Index f) {
        size_t pos = adjacency_index[f];
        mesh.foreach_facet_around_facet(f, [&](Index g) { adjacency_data[pos++] = g; });
    });

    return AdjacencyList<Index>(std::move(adjacency_data), std::move(adjacency_index));
}

#define LA_X_compute_facet_facet_adjacency(_, Scalar, Index)                 \
    template LA_CORE_API AdjacencyList<Index> compute_facet_facet_adjacency( \
        SurfaceMesh<Scalar, Index>&);
LA_SURFACE_MESH_X(compute_facet_facet_adjacency, 0)

} // namespace lagrange

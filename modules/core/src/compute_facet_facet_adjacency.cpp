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
#include <lagrange/types/ConnectivityType.h>
#include <lagrange/utils/assert.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <numeric>
#include <vector>

namespace lagrange {

namespace {

template <typename Scalar, typename Index>
AdjacencyList<Index> compute_facet_facet_adjacency_edge(SurfaceMesh<Scalar, Index>& mesh)
{
    mesh.initialize_edges();

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

template <typename Scalar, typename Index>
AdjacencyList<Index> compute_facet_facet_adjacency_vertex(SurfaceMesh<Scalar, Index>& mesh)
{
    mesh.initialize_edges();

    const Index num_facets = mesh.get_num_facets();

    using ValueArray = typename AdjacencyList<Index>::ValueArray;
    using IndexArray = typename AdjacencyList<Index>::IndexArray;

    // Count neighbors per facet by iterating through all vertices of each facet
    IndexArray adjacency_index(num_facets + 1, 0);
    tbb::parallel_for(Index(0), num_facets, [&](Index f) {
        // For each vertex in facet f, count how many other facets share that vertex
        for (Index c = mesh.get_facet_corner_begin(f); c != mesh.get_facet_corner_end(f); ++c) {
            Index v = mesh.get_corner_vertex(c);
            mesh.foreach_corner_around_vertex(v, [&](Index c2) {
                Index f2 = mesh.get_corner_facet(c2);
                if (f2 != f) {
                    adjacency_index[f + 1]++;
                }
            });
        }
    });

    // Prefix sum to get offsets.
    std::partial_sum(adjacency_index.begin(), adjacency_index.end(), adjacency_index.begin());

    // Fill adjacency data. pos is a local offset from adjacency_index[f]; adjacency_index is not
    // modified, so no shift-back is needed.
    ValueArray adjacency_data(adjacency_index.back());
    tbb::parallel_for(Index(0), num_facets, [&](Index f) {
        size_t pos = adjacency_index[f];
        for (Index c = mesh.get_facet_corner_begin(f); c != mesh.get_facet_corner_end(f); ++c) {
            Index v = mesh.get_corner_vertex(c);
            mesh.foreach_corner_around_vertex(v, [&](Index c2) {
                Index f2 = mesh.get_corner_facet(c2);
                if (f2 != f) {
                    adjacency_data[pos++] = f2;
                }
            });
        }
    });

    return AdjacencyList<Index>(std::move(adjacency_data), std::move(adjacency_index));
}

} // namespace

template <typename Scalar, typename Index>
AdjacencyList<Index> compute_facet_facet_adjacency(
    SurfaceMesh<Scalar, Index>& mesh,
    ConnectivityType connectivity_type)
{
    switch (connectivity_type) {
    case ConnectivityType::Edge: return compute_facet_facet_adjacency_edge(mesh);
    case ConnectivityType::Vertex: return compute_facet_facet_adjacency_vertex(mesh);
    default: la_runtime_assert(false, "Unsupported ConnectivityType for facet-facet adjacency");
    }
}

#define LA_X_compute_facet_facet_adjacency(_, Scalar, Index)                 \
    template LA_CORE_API AdjacencyList<Index> compute_facet_facet_adjacency( \
        SurfaceMesh<Scalar, Index>&,                                         \
        ConnectivityType);
LA_SURFACE_MESH_X(compute_facet_facet_adjacency, 0)

} // namespace lagrange

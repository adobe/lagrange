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
#include <lagrange/bvh/compute_intersecting_pairs.h>

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/bvh/TriangleAABBTree.h>
#include <lagrange/bvh/api.h>
#include <lagrange/utils/AdjacencyList.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/triangle_triangle_intersection.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <tbb/enumerable_thread_specific.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
#include <limits>
#include <numeric>

namespace lagrange::bvh {

template <typename Scalar, typename Index>
AdjacencyList<Index> compute_intersecting_pairs(const SurfaceMesh<Scalar, Index>& mesh)
{
    // Check that mesh is a triangle mesh
    if (!mesh.is_triangle_mesh()) {
        throw Error("compute_intersecting_pairs requires a triangle mesh");
    }

    const Index dim = mesh.get_dimension();
    if (dim != 3) {
        throw Error("compute_intersecting_pairs only supports 3D meshes");
    }

    const Index num_facets = mesh.get_num_facets();
    if (num_facets == 0) {
        using IndexArray = typename AdjacencyList<Index>::IndexArray;
        using ValueArray = typename AdjacencyList<Index>::ValueArray;
        return AdjacencyList<Index>(ValueArray(), IndexArray(1, 0));
    }

    // The underlying AABB tree uses uint32_t indices; verify the facet count fits
    if (num_facets > static_cast<Index>(std::numeric_limits<uint32_t>::max())) {
        throw Error(
            "compute_intersecting_pairs: mesh has too many facets for the AABB tree (max 2^32-1)");
    }

    logger().trace("Computing intersecting pairs for mesh with {} facets", num_facets);

    // Build AABB tree for the mesh
    TriangleAABBTree<Scalar, Index, 3> aabb_tree(mesh);

    // Get mesh data
    const auto vertices = vertex_view(mesh);

    // Thread-local storage for intersecting pairs found by each thread (flat list to avoid
    // O(num_facets * num_threads) memory allocation)
    using Pair = std::pair<Index, Index>;
    tbb::enumerable_thread_specific<std::vector<Pair>> thread_local_pairs;

    // Parallel loop over all facets
    tbb::parallel_for(Index(0), num_facets, [&](Index i) {
        // Get vertices of facet i
        auto facet_i_vertices = mesh.get_facet_vertices(i);
        la_debug_assert(facet_i_vertices.size() == 3);

        // Get positions of facet i vertices
        auto p0_i = vertices.row(facet_i_vertices[0]);
        auto p1_i = vertices.row(facet_i_vertices[1]);
        auto p2_i = vertices.row(facet_i_vertices[2]);

        // Compute bounding box for facet i
        typename TriangleAABBTree<Scalar, Index, 3>::AlignedBoxType bbox_i;
        bbox_i.setEmpty();
        bbox_i.extend(p0_i.transpose());
        bbox_i.extend(p1_i.transpose());
        bbox_i.extend(p2_i.transpose());

        // Local storage for this thread's intersecting pairs
        auto& local_pairs = thread_local_pairs.local();

        // Query AABB tree for potentially intersecting facets
        std::vector<uint32_t> candidates_raw;
        aabb_tree.get_aabb().intersect(bbox_i, candidates_raw);

        // Check each candidate facet
        for (uint32_t j_raw : candidates_raw) {
            Index j = static_cast<Index>(j_raw);
            // Only check j > i to avoid duplicates and self-intersection
            if (j <= i) continue;

            // No adjacency pre-filtering: triangle_triangle_intersection with
            // include_boundary=false correctly returns false for pairs that only
            // touch at shared vertices or edges, so adjacent facets are handled
            // by the geometric test without special-casing.

            // Get vertices of facet j
            auto facet_j_vertices = mesh.get_facet_vertices(j);
            la_debug_assert(facet_j_vertices.size() == 3);

            // Get positions of facet j vertices
            auto p0_j = vertices.row(facet_j_vertices[0]);
            auto p1_j = vertices.row(facet_j_vertices[1]);
            auto p2_j = vertices.row(facet_j_vertices[2]);

            // Check for intersection using exact predicates
            if (triangle_triangle_intersection(
                    span<const Scalar, 3>(p0_i.data(), 3),
                    span<const Scalar, 3>(p1_i.data(), 3),
                    span<const Scalar, 3>(p2_i.data(), 3),
                    span<const Scalar, 3>(p0_j.data(), 3),
                    span<const Scalar, 3>(p1_j.data(), 3),
                    span<const Scalar, 3>(p2_j.data(), 3))) {
                local_pairs.push_back({i, j});
            }
        }
    });

    // Merge results from all threads into per-facet neighbor lists
    using NeighborList = std::vector<Index>;
    std::vector<NeighborList> merged_neighbors(num_facets);
    for (const auto& local_pairs : thread_local_pairs) {
        for (const auto& [i, j] : local_pairs) {
            merged_neighbors[i].push_back(j);
            merged_neighbors[j].push_back(i);
        }
    }

    // Count total number of neighbors and build index array
    using IndexArray = typename AdjacencyList<Index>::IndexArray;
    using ValueArray = typename AdjacencyList<Index>::ValueArray;

    IndexArray adjacency_index(num_facets + 1, 0);
    for (Index i = 0; i < num_facets; ++i) {
        adjacency_index[i + 1] = static_cast<Index>(merged_neighbors[i].size());
    }
    std::partial_sum(adjacency_index.begin(), adjacency_index.end(), adjacency_index.begin());

    // Build value array
    ValueArray adjacency_data(adjacency_index.back());
    for (Index i = 0; i < num_facets; ++i) {
        std::copy(
            merged_neighbors[i].begin(),
            merged_neighbors[i].end(),
            adjacency_data.begin() + adjacency_index[i]);
    }

    logger().trace("Found {} intersecting facet pairs", adjacency_index.back() / 2);

    return AdjacencyList<Index>(std::move(adjacency_data), std::move(adjacency_index));
}

// Explicit template instantiations
#define LA_X_compute_intersecting_pairs(_, Scalar, Index)                               \
    template LA_BVH_API AdjacencyList<Index> compute_intersecting_pairs<Scalar, Index>( \
        const SurfaceMesh<Scalar, Index>&);
LA_SURFACE_MESH_X(compute_intersecting_pairs, 0)

} // namespace lagrange::bvh

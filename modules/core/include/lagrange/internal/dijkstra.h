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

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/legacy/internal/dijkstra.h>
#endif

#include <lagrange/SurfaceMesh.h>
#include <lagrange/utils/function_ref.h>
#include <lagrange/utils/span.h>

#include <Eigen/Core>

#include <queue>
#include <vector>

namespace lagrange::internal {

///
/// Reusable scratch buffers for Dijkstra traversal. Avoids per-call allocation when
/// dijkstra() is called repeatedly on the same mesh.
///
template <typename Scalar, typename Index>
struct DijkstraCache
{
    using Entry = std::pair<Scalar, Index>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
    std::vector<bool> visited;
    std::vector<bool> visited_edges;
    std::vector<bool> chord_bridged;
    std::vector<Index> edge_indices;
};

///
/// Options for the Dijkstra traversal.
///
template <typename Scalar>
struct DijkstraOptions
{
    /// Maximum geodesic distance from the seed. Vertices beyond this distance are not queued,
    /// unless they fall within the Euclidean distance (when enabled). A value <= 0 means no
    /// geodesic limit.
    Scalar geodesic_radius = 0;

    /// Maximum Euclidean distance from `seed_position`. When > 0, traversal continues while
    /// vertices are within the Euclidean distance, can bridge through out-of-scope vertices if the
    /// chordal distance is still within Euclidean limits.
    Scalar euclidean_radius = 0;

    /// Center of the Euclidean ball. Required when `euclidean_radius > 0`.
    Eigen::Vector3<Scalar> seed_position = Eigen::Vector3<Scalar>::Zero();
};

///
/// Traverse the mesh based on Dijkstra's algorithm with customized distance metric and process
/// functions.
///
/// @param      mesh                The input mesh.
/// @param      seed_vertices       Seed vertices.
/// @param      seed_vertex_dist    Initial distance to the seed vertices. Assumed to be correct/minimal.
/// @param      dijkstra_options    Options controlling the traversal radius and behavior.
/// @param      dist                The distance metric.  e.g. `d = dist(v0, v1)`
/// @param      process             Callback for each reached vertex: `process(vid, v_dist)`
/// @param      cache               Optional reusable scratch buffers to avoid per-call
///                                 allocation. When non-null, vectors are resized/reset at the
///                                 start of the call but their capacity is preserved across
///                                 calls. When null, a local cache is allocated.
///
/// @tparam     Scalar              Mesh scalar type
/// @tparam     Index               Mesh index type
///
template <typename Scalar, typename Index>
void dijkstra(
    SurfaceMesh<Scalar, Index>& mesh,
    span<const Index> seed_vertices,
    span<const Scalar> seed_vertex_dist,
    const DijkstraOptions<Scalar>& dijkstra_options,
    const function_ref<Scalar(Index, Index)>& dist,
    const function_ref<void(Index, Scalar)>& process,
    DijkstraCache<Scalar, Index>* cache = nullptr);

} // namespace lagrange::internal

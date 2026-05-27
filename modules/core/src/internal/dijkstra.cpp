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

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/point_segment_squared_distance.h>
#include <lagrange/views.h>

namespace lagrange::internal {

namespace {

///
/// Iterates over each pair of consecutive edges (within an incident facet) around a vertex.
///
template <typename Scalar, typename Index, typename Func>
void foreach_edge_pair_around_vertex_with_duplicates(
    const SurfaceMesh<Scalar, Index>& mesh,
    Index v,
    Func&& func)
{
    mesh.foreach_corner_around_vertex(v, [&](Index c) {
        const Index f = mesh.get_corner_facet(c);
        const Index c_start = mesh.get_facet_corner_begin(f);
        const Index nv = mesh.get_facet_size(f);
        const Index lv_curr = c - c_start;
        const Index lv_prev = (lv_curr + nv - 1) % nv;
        const Index e_curr = mesh.get_corner_edge(c_start + lv_curr);
        const Index e_prev = mesh.get_corner_edge(c_start + lv_prev);
        func(e_curr, e_prev);
    });
}

///
/// Shared implementation used by both the geodesic-only and the geodesic+Euclidean variants.
/// The neighbor expansion is delegated to `explore(vi, di)`, which is expected to push
/// reachable neighbors onto `cache.queue`. `in_scope(vi, di)` decides whether `process` fires
/// for a popped vertex; if `always_explore` is true, `explore` also runs for out-of-scope pops
/// (used by the chord-bridge variant). `terminate_on_out_of_scope` enables an early-out for the
/// geodesic-only variant, whose priority queue is monotonic in geodesic distance.
///
template <typename Scalar, typename Index, typename InScope, typename Explore>
void dijkstra_impl(
    SurfaceMesh<Scalar, Index>& mesh,
    span<const Index> seed_vertices,
    span<const Scalar> seed_vertex_dist,
    DijkstraCache<Scalar, Index>* cache_ptr,
    bool always_explore,
    bool terminate_on_out_of_scope,
    InScope&& in_scope,
    const function_ref<void(Index, Scalar)>& process,
    Explore&& explore)
{
    DijkstraCache<Scalar, Index> local_cache;
    DijkstraCache<Scalar, Index>& cache = cache_ptr ? *cache_ptr : local_cache;

    mesh.initialize_edges();

    const auto num_vertices = mesh.get_num_vertices();
    const auto num_edges = mesh.get_num_edges();

    auto& Q = cache.queue;
    while (!Q.empty()) Q.pop();
    cache.visited.assign(num_vertices, false);
    cache.visited_edges.assign(num_edges, false);
    cache.chord_bridged.assign(num_vertices, false);
    cache.edge_indices.clear();
    cache.edge_indices.reserve(16);

    size_t num_seeds = seed_vertices.size();
    la_runtime_assert(num_seeds == seed_vertex_dist.size());

    // Seed vertices are always processed and always explore their neighbors,
    // regardless of whether their initial distance exceeds the radius.
    // Initial distances are assumed to be correct/minimal, so we
    // already mark seed vertices as visited.
    for (size_t i = 0; i < num_seeds; i++) {
        Index vi = seed_vertices[i];
        Scalar di = seed_vertex_dist[i];
        la_runtime_assert(vi < num_vertices);
        if (cache.visited[vi]) continue;
        cache.visited[vi] = true;

        process(vi, di);
        explore(vi, di);
    }

    while (!Q.empty()) {
        auto entry = Q.top();
        Q.pop();

        Index vi = entry.second;
        Scalar di = entry.first;

        if (cache.visited[vi]) continue;
        cache.visited[vi] = true;

        if (in_scope(vi, di)) {
            process(vi, di);
        } else if (!always_explore) {
            if (terminate_on_out_of_scope) break;
            continue;
        }

        explore(vi, di);
    }
}

// Geodesic-only traversal: enqueue a neighbor only if it stays within the geodesic ball, and
// break out as soon as the priority queue head exceeds the radius.
template <typename Scalar, typename Index>
void dijkstra_geodesic_only(
    SurfaceMesh<Scalar, Index>& mesh,
    span<const Index> seed_vertices,
    span<const Scalar> seed_vertex_dist,
    Scalar geo_radius,
    const function_ref<Scalar(Index, Index)>& dist,
    const function_ref<void(Index, Scalar)>& process,
    DijkstraCache<Scalar, Index>& cache)
{
    auto in_scope = [&](Index /*vi*/, Scalar di) { return di <= geo_radius; };

    auto explore = [&](Index vi, Scalar di) {
        cache.edge_indices.clear();
        mesh.foreach_edge_around_vertex_with_duplicates(vi, [&](Index ei) {
            if (cache.visited_edges[ei]) return;
            cache.visited_edges[ei] = true;
            cache.edge_indices.push_back(ei);

            auto e = mesh.get_edge_vertices(ei);
            Index vj = (e[0] == vi) ? e[1] : e[0];
            Scalar dj = di + dist(vi, vj);
            if (dj <= geo_radius) {
                cache.queue.push({dj, vj});
            }
        });
        for (auto ei : cache.edge_indices) {
            cache.visited_edges[ei] = false;
        }
    };

    dijkstra_impl<Scalar, Index>(
        mesh,
        seed_vertices,
        seed_vertex_dist,
        &cache,
        /*always_explore=*/false,
        /*terminate_on_out_of_scope=*/true,
        in_scope,
        process,
        explore);
}

// Geodesic + Euclidean traversal: vertices outside both balls are still allowed to bridge to
// neighbors via the chord-in-scope check, recovering reachable in-scope vertices that the
// pure geodesic variant would miss when the only path passes through out-of-scope vertices.
template <typename Scalar, typename Index>
void dijkstra_geodesic_and_euclidean(
    SurfaceMesh<Scalar, Index>& mesh,
    span<const Index> seed_vertices,
    span<const Scalar> seed_vertex_dist,
    Scalar geo_radius,
    Scalar euclidean_radius_sq,
    const Eigen::Vector3<Scalar>& seed_position,
    const function_ref<Scalar(Index, Index)>& dist,
    const function_ref<void(Index, Scalar)>& process,
    DijkstraCache<Scalar, Index>& cache)
{
    const auto vertices = vertex_view(mesh);

    auto vertex_in_eucl_ball = [&](Index vi) -> bool {
        return (Eigen::Vector3<Scalar>(vertices.row(vi)) - seed_position).squaredNorm() <=
               euclidean_radius_sq;
    };

    // Distance from `seed_position` to the segment (a, b) <= euclidean_radius? Used as the
    // chord-bridge predicate: when true, both endpoints get enqueued from the current vertex
    // even if the current vertex itself sits outside both balls.
    auto chord_in_eucl_ball = [&](Index va, Index vb) -> bool {
        Eigen::Vector3<Scalar> pa = vertices.row(va);
        Eigen::Vector3<Scalar> pb = vertices.row(vb);
        return point_segment_squared_distance(seed_position, pa, pb) <= euclidean_radius_sq;
    };

    // A chord-bridged vertex is also considered in scope, so it is reported via `process()` when
    // popped at its minimum graph distance (the `cache.visited` flag deduplicates the call).
    auto in_scope = [&](Index vi, Scalar di) {
        return di <= geo_radius || vertex_in_eucl_ball(vi) || cache.chord_bridged[vi];
    };

    auto explore = [&](Index vi, Scalar di) {
        cache.edge_indices.clear();
        foreach_edge_pair_around_vertex_with_duplicates(mesh, vi, [&](Index e_curr, Index e_prev) {
            // The two non-vi endpoints of this facet corner form the chord opposite vi.
            auto ec = mesh.get_edge_vertices(e_curr);
            auto ep = mesh.get_edge_vertices(e_prev);
            Index v_curr = (ec[0] == vi) ? ec[1] : ec[0];
            Index v_prev = (ep[0] == vi) ? ep[1] : ep[0];

            // Chord bridge: when the chord (v_curr, v_prev) stays inside the Euclidean
            // ball, we may need to enqueue both endpoints even if they are individually
            // out of scope, so the search can keep advancing through the edge.
            bool chord_bridge = (v_curr != v_prev) && chord_in_eucl_ball(v_curr, v_prev);
            if (chord_bridge) {
                cache.chord_bridged[v_curr] = true;
                cache.chord_bridged[v_prev] = true;
            }

            auto try_enqueue = [&](Index ei, Index vj) {
                if (!chord_bridge && cache.visited_edges[ei]) return;
                cache.visited_edges[ei] = true;
                cache.edge_indices.push_back(ei);

                Scalar dj = di + dist(vi, vj);
                if (chord_bridge || in_scope(vj, dj)) {
                    cache.queue.push({dj, vj});
                }
            };
            try_enqueue(e_curr, v_curr);
            try_enqueue(e_prev, v_prev);
        });
        for (auto ei : cache.edge_indices) {
            cache.visited_edges[ei] = false;
        }
    };

    dijkstra_impl<Scalar, Index>(
        mesh,
        seed_vertices,
        seed_vertex_dist,
        &cache,
        /*always_explore=*/true,
        /*terminate_on_out_of_scope=*/false,
        in_scope,
        process,
        explore);
}

} // namespace

template <typename Scalar, typename Index>
void dijkstra(
    SurfaceMesh<Scalar, Index>& mesh,
    span<const Index> seed_vertices,
    span<const Scalar> seed_vertex_dist,
    const DijkstraOptions<Scalar>& opts,
    const function_ref<Scalar(Index, Index)>& dist,
    const function_ref<void(Index, Scalar)>& process,
    DijkstraCache<Scalar, Index>* cache_ptr)
{
    DijkstraCache<Scalar, Index> local_cache;
    DijkstraCache<Scalar, Index>& cache = cache_ptr ? *cache_ptr : local_cache;

    const Scalar geo_radius = opts.geodesic_radius > Scalar(0) ? opts.geodesic_radius
                                                               : std::numeric_limits<Scalar>::max();

    if (opts.euclidean_radius > Scalar(0)) {
        const Scalar euclidean_radius_sq = opts.euclidean_radius * opts.euclidean_radius;
        dijkstra_geodesic_and_euclidean<Scalar, Index>(
            mesh,
            seed_vertices,
            seed_vertex_dist,
            geo_radius,
            euclidean_radius_sq,
            opts.seed_position,
            dist,
            process,
            cache);
    } else {
        dijkstra_geodesic_only<Scalar, Index>(
            mesh,
            seed_vertices,
            seed_vertex_dist,
            geo_radius,
            dist,
            process,
            cache);
    }
}

#define LA_X_dijkstra(_, Scalar, Index)                \
    template LA_CORE_API void dijkstra<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                   \
        span<const Index>,                             \
        span<const Scalar>,                            \
        const DijkstraOptions<Scalar>&,                \
        const function_ref<Scalar(Index, Index)>&,     \
        const function_ref<void(Index, Scalar)>&,      \
        DijkstraCache<Scalar, Index>*);

LA_SURFACE_MESH_X(dijkstra, 0)

} // namespace lagrange::internal

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
#include <lagrange/bvh/resolve_tjunctions.h>

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/bvh/api.h>
#include <lagrange/internal/split_edges.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/function_ref.h>
#include <lagrange/utils/span.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_sort.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
#include <numeric>
#include <utility>
#include <vector>

namespace lagrange::bvh {

template <typename Scalar, typename Index>
void resolve_tjunctions(SurfaceMesh<Scalar, Index>& mesh, ResolveTJunctionsOptions options)
{
    mesh.initialize_edges();

    const Index num_vertices = mesh.get_num_vertices();
    const Index num_edges = mesh.get_num_edges();
    const Index dim = mesh.get_dimension();
    if (num_vertices == 0 || num_edges == 0) return;

    auto vertices = vertex_view(mesh);
    auto extent = (vertices.colwise().maxCoeff() - vertices.colwise().minCoeff()).eval();

    // Resolve the detection tolerance (default is relative to the bounding box diagonal).
    Scalar tol = static_cast<Scalar>(options.tolerance);
    if (options.tolerance < 0) {
        tol = static_cast<Scalar>(1e-6) * extent.norm();
    }
    if (tol < 0) tol = 0;
    const Scalar tol_sq = tol * tol;

    // Pick the axis with the largest extent to drive the sort-and-sweep candidate search.
    Index axis = 0;
    {
        Scalar best = extent(0);
        for (Index d = 1; d < dim; d++) {
            if (extent(d) > best) {
                best = extent(d);
                axis = d;
            }
        }
    }

    // Sort vertex ids by their coordinate along the dominant axis.
    std::vector<Index> order(num_vertices);
    std::iota(order.begin(), order.end(), Index(0));
    tbb::parallel_sort(order.begin(), order.end(), [&](Index a, Index b) {
        return vertices(a, axis) < vertices(b, axis);
    });
    std::vector<Scalar> sorted_axis(num_vertices);
    for (Index i = 0; i < num_vertices; i++) sorted_axis[i] = vertices(order[i], axis);

    // Report each vertex on edge `e` (within tolerance, strictly between endpoints) to `visit`.
    auto for_each_split_on_edge = [&](Index e, auto&& visit) {
        if (options.boundary_only && !mesh.is_boundary_edge(e)) return;
        auto ev = mesh.get_edge_vertices(e);
        const Index v0 = ev[0];
        const Index v1 = ev[1];
        auto p0 = vertices.row(v0);
        auto p1 = vertices.row(v1);
        auto edge_dir = (p1 - p0).eval();
        const Scalar len_sq = edge_dir.squaredNorm();
        if (len_sq <= 0) return; // degenerate edge

        const Scalar lo = std::min(p0(axis), p1(axis)) - tol;
        const Scalar hi = std::max(p0(axis), p1(axis)) + tol;
        auto it_begin = std::lower_bound(sorted_axis.begin(), sorted_axis.end(), lo);
        auto it_end = std::upper_bound(sorted_axis.begin(), sorted_axis.end(), hi);
        for (auto it = it_begin; it != it_end; ++it) {
            const Index v = order[static_cast<size_t>(std::distance(sorted_axis.begin(), it))];
            if (v == v0 || v == v1) continue;
            auto pv = vertices.row(v);
            const Scalar t = (pv - p0).dot(edge_dir) / len_sq;
            if (t <= 0 || t >= 1) continue; // must lie strictly between endpoints
            const Scalar dist_sq = (pv - (p0 + t * edge_dir)).squaredNorm();
            if (dist_sq > tol_sq) continue;
            visit(t, v);
        }
    };

    // Pass 1: count split points per edge, then prefix-sum into CSR offsets.
    std::vector<Index> edge_split_offsets(num_edges + 1, 0);
    tbb::parallel_for(Index(0), num_edges, [&](Index e) {
        Index count = 0;
        for_each_split_on_edge(e, [&](Scalar, Index) { ++count; });
        edge_split_offsets[e + 1] = count;
    });
    for (Index e = 0; e < num_edges; e++) edge_split_offsets[e + 1] += edge_split_offsets[e];

    const Index num_split_pts = edge_split_offsets[num_edges];
    if (num_split_pts == 0) return;

    // Pass 2: fill each edge's range, ordered from get_edge_vertices(e)[0] to [1] (ascending t).
    std::vector<std::pair<Scalar, Index>> split_scratch(num_split_pts);
    std::vector<Index> split_pts(num_split_pts);
    tbb::parallel_for(Index(0), num_edges, [&](Index e) {
        const Index begin = edge_split_offsets[e];
        const Index end = edge_split_offsets[e + 1];
        Index cursor = begin;
        for_each_split_on_edge(e, [&](Scalar t, Index v) { split_scratch[cursor++] = {t, v}; });
        std::sort(
            split_scratch.begin() + begin,
            split_scratch.begin() + end,
            [](const auto& a, const auto& b) { return a.first < b.first; });
        for (Index i = begin; i < end; i++) split_pts[i] = split_scratch[i].second;
    });

    // Split edges without retriangulating: each affected facet gets a polygonal copy appended at
    // id >= old_num_facets, leaving the originals (to be removed) in place.
    const Index old_num_facets = mesh.get_num_facets();
    auto facets_to_remove = lagrange::internal::split_edges_only(
        mesh,
        function_ref<span<Index>(Index)>([&](Index e) -> span<Index> {
            const Index n = edge_split_offsets[e + 1] - edge_split_offsets[e];
            return span<Index>(split_pts.data() + edge_split_offsets[e], n);
        }),
        function_ref<bool(Index)>([](Index) { return true; }));

    // Optionally triangulate only the new facets, then drop the original split facets.
    if (options.triangulate_affected) {
        auto is_new_facet = [old_num_facets](Index f) { return f >= old_num_facets; };
        triangulate_polygonal_facets(mesh, function_ref<bool(Index)>(is_new_facet));
    }
    mesh.remove_facets(facets_to_remove);
}

#define LA_X_resolve_tjunctions(_, Scalar, Index)               \
    template LA_BVH_API void resolve_tjunctions<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                            \
        ResolveTJunctionsOptions);
LA_SURFACE_MESH_X(resolve_tjunctions, 0)

} // namespace lagrange::bvh

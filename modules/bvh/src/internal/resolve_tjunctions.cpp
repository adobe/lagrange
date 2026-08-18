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
#include <lagrange/bvh/internal/resolve_tjunctions.h>

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/bvh/AABB.h>
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
#include <lagrange/utils/warnon.h>
// clang-format on

#include <Eigen/Geometry>

#include <algorithm>
#include <utility>
#include <vector>

namespace lagrange::bvh::internal {

namespace {

template <typename Scalar, typename Index, int Dim>
void resolve_tjunctions_impl(
    SurfaceMesh<Scalar, Index>& mesh,
    const ResolveTJunctionsOptions& options)
{
    using Tree = AABB<Scalar, Dim>;
    using Box = typename Tree::Box;
    using Point = typename Tree::Point;

    mesh.initialize_edges();
    const Index num_vertices = mesh.get_num_vertices();
    const Index num_edges = mesh.get_num_edges();
    if (num_vertices == 0 || num_edges == 0) return;

    auto vertices = vertex_view(mesh);
    auto to_point = [&](Index v) {
        Point p;
        for (int d = 0; d < Dim; ++d) p[d] = vertices(v, d);
        return p;
    };

    // Resolve the detection tolerance (default is relative to the bounding box diagonal).
    Scalar tol = static_cast<Scalar>(options.tolerance);
    if (options.tolerance < 0) {
        Scalar diag = (vertices.colwise().maxCoeff() - vertices.colwise().minCoeff()).norm();
        tol = static_cast<Scalar>(1e-6) * diag;
    }
    if (tol < 0) tol = 0;
    const Scalar tol_sq = tol * tol;

    // Build an AABB tree over the vertices (each stored as a degenerate point box).
    std::vector<Box> boxes(static_cast<size_t>(num_vertices));
    for (Index v = 0; v < num_vertices; ++v) {
        const Point p = to_point(v);
        boxes[v] = Box(p, p);
    }
    Tree tree;
    tree.build({boxes.data(), boxes.size()});

    // For each edge, query the tree with the edge's tolerance-expanded box for candidate vertices.
    std::vector<std::vector<std::pair<Scalar, Index>>> edge_splits(num_edges);
    tbb::parallel_for(Index(0), num_edges, [&](Index e) {
        if (options.boundary_only && !mesh.is_boundary_edge(e)) return;
        auto ev = mesh.get_edge_vertices(e);
        const Index v0 = ev[0];
        const Index v1 = ev[1];
        const Point p0 = to_point(v0);
        const Point p1 = to_point(v1);
        const Point edge_dir = p1 - p0;
        const Scalar len_sq = edge_dir.squaredNorm();
        if (len_sq <= 0) return; // degenerate edge

        Box query(p0, p0);
        query.extend(p1);
        query =
            Box((query.min() - Point::Constant(tol)).eval(),
                (query.max() + Point::Constant(tol)).eval());

        tree.intersect(
            query,
            function_ref<bool(typename Tree::Index)>([&](typename Tree::Index candidate) {
                const Index v = static_cast<Index>(candidate);
                if (v == v0 || v == v1) return true;
                const Point pv = to_point(v);
                const Scalar t = (pv - p0).dot(edge_dir) / len_sq;
                if (t <= 0 || t >= 1) return true; // must lie strictly between endpoints
                const Scalar dist_sq = (pv - (p0 + t * edge_dir)).squaredNorm();
                if (dist_sq > tol_sq) return true;
                edge_splits[e].emplace_back(t, v);
                return true;
            }));
        std::sort(edge_splits[e].begin(), edge_splits[e].end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
    });

    // Build CSR split lists, ordered from get_edge_vertices(e)[0] to [1] (ascending t).
    std::vector<Index> edge_split_offsets(num_edges + 1, 0);
    std::vector<Index> split_pts;
    for (Index e = 0; e < num_edges; e++) {
        for (const auto& entry : edge_splits[e]) split_pts.push_back(entry.second);
        edge_split_offsets[e + 1] = static_cast<Index>(split_pts.size());
    }

    if (split_pts.empty()) return;

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

} // namespace

template <typename Scalar, typename Index>
void resolve_tjunctions(SurfaceMesh<Scalar, Index>& mesh, ResolveTJunctionsOptions options)
{
    const Index dim = mesh.get_dimension();
    if (dim == 2) {
        resolve_tjunctions_impl<Scalar, Index, 2>(mesh, options);
    } else if (dim == 3) {
        resolve_tjunctions_impl<Scalar, Index, 3>(mesh, options);
    } else {
        la_runtime_assert(false, "resolve_tjunctions: only 2D and 3D meshes are supported.");
    }
}

#define LA_X_resolve_tjunctions(_, Scalar, Index)               \
    template LA_BVH_API void resolve_tjunctions<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                            \
        ResolveTJunctionsOptions);
LA_SURFACE_MESH_X(resolve_tjunctions, 0)

} // namespace lagrange::bvh::internal

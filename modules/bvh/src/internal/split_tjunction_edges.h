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
#pragma once

#include <lagrange/SurfaceMesh.h>
#include <lagrange/internal/split_edges.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/function_ref.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/span.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <vector>

namespace lagrange::bvh::internal {

enum : std::uint8_t { AffectedTriangle = 1, AffectedPolygon = 2 };

template <typename Scalar, typename Index>
void classify_affected_facets(
    SurfaceMesh<Scalar, Index>& mesh,
    Index edge,
    std::atomic<std::uint8_t>& affected_types)
{
    const auto known = affected_types.load(std::memory_order_relaxed);
    if (known == (AffectedTriangle | AffectedPolygon)) return;
    std::uint8_t found = 0;
    for (Index c = mesh.get_first_corner_around_edge(edge); c != invalid<Index>();
         c = mesh.get_next_corner_around_edge(c)) {
        found |=
            mesh.get_facet_size(mesh.get_corner_facet(c)) == 3 ? AffectedTriangle : AffectedPolygon;
        if ((known | found) == (AffectedTriangle | AffectedPolygon)) break;
    }
    if (found & ~known) affected_types.fetch_or(found, std::memory_order_relaxed);
}

// Both splitters retain the original facets, so their IDs remain stable until the final removal.
// Only the mixed case needs a second pass and an edge-ID remap after split_edges clears the edges.
template <typename Scalar, typename Index>
void split_tjunction_edges(
    SurfaceMesh<Scalar, Index>& mesh,
    function_ref<span<Index>(Index)> get_edge_split_pts,
    bool triangulate_affected,
    std::uint8_t affected_types)
{
    auto all_active = [](Index) { return true; };
    std::vector<Index> facets_to_remove;

    if (!triangulate_affected || !(affected_types & AffectedTriangle)) {
        const Index old_num_facets = mesh.get_num_facets();
        facets_to_remove = lagrange::internal::split_edges_only(
            mesh,
            get_edge_split_pts,
            function_ref<bool(Index)>(all_active));
        if (triangulate_affected) {
            auto is_new_facet = [old_num_facets](Index f) { return f >= old_num_facets; };
            TriangulationOptions options;
            options.scheme = TriangulationOptions::Scheme::Delaunay;
            triangulate_polygonal_facets(
                mesh,
                function_ref<bool(Index)>(is_new_facet),
                std::move(options));
        }
    } else if (!(affected_types & AffectedPolygon)) {
        facets_to_remove = lagrange::internal::split_edges(
            mesh,
            get_edge_split_pts,
            function_ref<bool(Index)>(all_active));
    } else {
        const Index original_num_facets = mesh.get_num_facets();

        // In the very rare case that the region affected by T-junctions contains both triangles
        // and polygons, we need to split the edges in two passes. Unfortunately, extra book
        // keeping is needed to remap the split points of the original polygon edges after
        // split_edges clears the edge table.
        struct SplitPolygonEdge
        {
            Index edge;
            Index v0;
            Index v1;
        };
        std::vector<SplitPolygonEdge> polygon_edges;
        for (Index e = 0; e < mesh.get_num_edges(); ++e) {
            if (get_edge_split_pts(e).empty()) continue;
            for (Index c = mesh.get_first_corner_around_edge(e); c != invalid<Index>();
                 c = mesh.get_next_corner_around_edge(c)) {
                if (mesh.get_facet_size(mesh.get_corner_facet(c)) == 3) continue;
                const auto ev = mesh.get_edge_vertices(e);
                polygon_edges.push_back({e, ev[0], ev[1]});
                break;
            }
        }

        auto is_original_triangle = [&](Index f) {
            return f < original_num_facets && mesh.get_facet_size(f) == 3;
        };
        facets_to_remove = lagrange::internal::split_edges(
            mesh,
            get_edge_split_pts,
            function_ref<bool(Index)>(is_original_triangle));

        // split_edges clears the edge table. The original polygon facets still carry their old
        // edges; rebuild it and map only their split edges to the original CSR ranges.
        mesh.initialize_edges();
        std::vector<Index> old_edge_for_new_edge(mesh.get_num_edges(), invalid<Index>());
        for (const auto& old : polygon_edges) {
            const Index e = mesh.find_edge_from_vertices(old.v0, old.v1);
            la_debug_assert(e != invalid<Index>());
            old_edge_for_new_edge[e] = old.edge;
            if (mesh.get_edge_vertices(e)[0] != old.v0) {
                auto pts = get_edge_split_pts(old.edge);
                std::reverse(pts.begin(), pts.end());
            }
        }
        auto get_remapped_split_pts = [&](Index e) -> span<Index> {
            const Index old = old_edge_for_new_edge[e];
            return old == invalid<Index>() ? span<Index>() : get_edge_split_pts(old);
        };
        auto is_original_polygon = [&](Index f) {
            return f < original_num_facets && mesh.get_facet_size(f) != 3;
        };
        const Index before_polygons = mesh.get_num_facets();
        auto polygons_to_remove = lagrange::internal::split_edges_only(
            mesh,
            function_ref<span<Index>(Index)>(get_remapped_split_pts),
            function_ref<bool(Index)>(is_original_polygon));
        auto is_new_polygon = [before_polygons](Index f) { return f >= before_polygons; };
        TriangulationOptions options;
        options.scheme = TriangulationOptions::Scheme::Delaunay;
        triangulate_polygonal_facets(
            mesh,
            function_ref<bool(Index)>(is_new_polygon),
            std::move(options));
        facets_to_remove.insert(
            facets_to_remove.end(),
            polygons_to_remove.begin(),
            polygons_to_remove.end());
        std::sort(facets_to_remove.begin(), facets_to_remove.end());
    }
    mesh.remove_facets(facets_to_remove);
}

} // namespace lagrange::bvh::internal

/*
 * Copyright 2023 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/topology.h>

#include <lagrange/Attribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/blocked_range.h>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <map>

namespace lagrange {

template <typename Scalar, typename Index>
int compute_euler(const SurfaceMesh<Scalar, Index>& mesh)
{
    if (mesh.has_edges()) {
        // Need to cast to int because Index may not be signed.
        return static_cast<int>(mesh.get_num_vertices()) - static_cast<int>(mesh.get_num_edges()) +
               static_cast<int>(mesh.get_num_facets());
    } else {
        auto mesh_copy = mesh;
        mesh_copy.initialize_edges();
        return static_cast<int>(mesh_copy.get_num_vertices()) -
               static_cast<int>(mesh_copy.get_num_edges()) +
               static_cast<int>(mesh_copy.get_num_facets());
    }
}

namespace {

// Walk corners around a vertex based on adjacency.
template <typename Scalar, typename Index>
size_t
count_connected_corners(const SurfaceMesh<Scalar, Index>& mesh, Index vid, Index starting_corner)
{
    la_debug_assert(mesh.get_corner_vertex(starting_corner) == vid);
    const size_t max_count = mesh.count_num_corners_around_vertex(vid);
    size_t count = 0;
    Index ci = starting_corner;
    do {
        la_debug_assert(mesh.get_corner_vertex(ci) == vid);
        count++;
        la_runtime_assert(count <= max_count, "Infinite loop detected.");
        Index fi = mesh.get_corner_facet(ci);
        Index c0 = mesh.get_facet_corner_begin(fi);
        Index li = ci - c0;
        Index ni = mesh.get_facet_size(fi);
        Index cj = c0 + (li + ni - 1) % ni;
        ci = mesh.get_next_corner_around_edge(cj);
        if (ci == invalid<Index>()) {
            Index ej = mesh.get_corner_edge(cj);
            ci = mesh.get_first_corner_around_edge(ej);
            if (ci == cj) break; // Boundary edge.
        }
    } while (ci != starting_corner);
    return count;
};

template <typename Scalar, typename Index>
bool is_vertex_manifold(
    const SurfaceMesh<Scalar, Index>& mesh,
    Index vid,
    std::multimap<Index, size_t>& rim_edges)
{
    rim_edges.clear();

    for (auto ci = mesh.get_first_corner_around_vertex(vid); ci != invalid<Index>();
         ci = mesh.get_next_corner_around_vertex(ci)) {
        Index fid = mesh.get_corner_facet(ci);
        for (auto cj = mesh.get_facet_corner_begin(fid); cj != mesh.get_facet_corner_end(fid);
             cj++) {
            Index ck = cj + 1;
            if (ck == mesh.get_facet_corner_end(fid)) ck = mesh.get_facet_corner_begin(fid);
            Index vj = mesh.get_corner_vertex(cj);
            Index vk = mesh.get_corner_vertex(ck);
            if (vj == vid || vk == vid) {
                continue;
            }
            rim_edges.insert({vj, rim_edges.size()});
            rim_edges.insert({vk, rim_edges.size()});
        }
    }
    if (rim_edges.empty()) return true;

    // An interior vertex is manifold iff each of its neighboring vertex has one incoming rim
    // edge and one outgoing rim edge.  For vertices on the mesh boundary, it should has exactly
    // two neighboring vertices that are end points.
    size_t end_points = 0;
    for (const auto& entry : rim_edges) {
        size_t count = rim_edges.count(entry.first);
        if (count == 1) {
            end_points++;
        } else if (count == 2) {
            auto itr1 = rim_edges.lower_bound(entry.first);
            auto itr2 = std::next(itr1);
            // Vertices with incoming rim edges have odd rim indices.
            // Vertices with outgoing rim edges have even rim indices.
            if ((itr1->second + itr2->second) % 2 == 0) return false;
        } else {
            return false;
        }
    }

    if (end_points == 0) {
        return mesh.count_num_corners_around_vertex(vid) ==
               count_connected_corners(mesh, vid, mesh.get_first_corner_around_vertex(vid));
    } else if (end_points == 2) {
        Index starting_corner = invalid<Index>();
        for (Index ci = mesh.get_first_corner_around_vertex(vid); ci != invalid<Index>();
             ci = mesh.get_next_corner_around_vertex(ci)) {
            Index ei = mesh.get_corner_edge(ci);
            if (mesh.is_boundary_edge(ei)) {
                starting_corner = ci;
                break;
            }
        }
        if (starting_corner == invalid<Index>()) return false;
        return mesh.count_num_corners_around_vertex(vid) ==
               count_connected_corners(mesh, vid, starting_corner);
    } else {
        return false;
    }
    return end_points == 0 || end_points == 2;
};

} // namespace

template <typename Scalar, typename Index>
bool is_vertex_manifold(const SurfaceMesh<Scalar, Index>& mesh)
{
    if (!mesh.has_edges()) {
        auto mesh_copy = mesh;
        mesh_copy.initialize_edges();
        return is_vertex_manifold(mesh_copy);
    }

    const auto num_vertices = mesh.get_num_vertices();
    // TODO: Avoid multimap by ensuring all spoke edges are manifold and consistently oriented
    // instead.
    tbb::enumerable_thread_specific<std::multimap<Index, size_t>> thread_rim_edges;

    return tbb::parallel_reduce(
        tbb::blocked_range<Index>(0, num_vertices),
        true, ///< initial value of the result.
        [&](const tbb::blocked_range<Index>& r, bool manifold) -> bool {
            if (!manifold) return false;

            for (Index vi = r.begin(); vi != r.end(); vi++) {
                if (!is_vertex_manifold(mesh, vi, thread_rim_edges.local())) {
                    return false;
                }
            }
            return true;
        },
        std::logical_and<bool>());
}

template <typename Scalar, typename Index>
bool is_edge_manifold(const SurfaceMesh<Scalar, Index>& mesh)
{
    if (!mesh.has_edges()) {
        auto mesh_copy = mesh;
        mesh_copy.initialize_edges();
        return is_edge_manifold(mesh_copy);
    }

    const auto num_edges = mesh.get_num_edges();
    for (Index ei = 0; ei < num_edges; ei++) {
        auto num_corners_around_edge = mesh.count_num_corners_around_edge(ei);
        if (num_corners_around_edge > 2) return false;
    }
    return true;
}

template <typename Scalar, typename Index>
AttributeId compute_vertex_is_manifold(
    SurfaceMesh<Scalar, Index>& mesh,
    const VertexManifoldOptions& options)
{
    AttributeId id = internal::find_or_create_attribute<uint8_t>(
        mesh,
        options.output_attribute_name,
        Vertex,
        AttributeUsage::Scalar,
        1,
        internal::ResetToDefault::No);
    auto attr = mesh.template ref_attribute<uint8_t>(id).ref_all();

    // TODO: Avoid multimap by ensuring all spoke edges are manifold and consistently oriented
    // instead.
    tbb::enumerable_thread_specific<std::multimap<Index, size_t>> thread_rim_edges;

    tbb::parallel_for(
        tbb::blocked_range<Index>(0, mesh.get_num_vertices()),
        [&](const tbb::blocked_range<Index>& r) {
            for (Index vi = r.begin(); vi != r.end(); vi++) {
                attr[vi] = is_vertex_manifold(mesh, vi, thread_rim_edges.local());
            }
        });

    return id;
}

#define LA_X_topology(_, Scalar, Index)                                                            \
    template LA_CORE_API int compute_euler<Scalar, Index>(const SurfaceMesh<Scalar, Index>& mesh); \
    template LA_CORE_API bool is_vertex_manifold<Scalar, Index>(                                   \
        const SurfaceMesh<Scalar, Index>& mesh);                                                   \
    template LA_CORE_API bool is_edge_manifold<Scalar, Index>(                                     \
        const SurfaceMesh<Scalar, Index>& mesh);                                                   \
    template LA_CORE_API AttributeId compute_vertex_is_manifold(                                   \
        SurfaceMesh<Scalar, Index>& mesh,                                                          \
        const VertexManifoldOptions& options);
LA_SURFACE_MESH_X(topology, 0)

} // namespace lagrange

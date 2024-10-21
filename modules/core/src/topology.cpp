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

template <typename Scalar, typename Index>
bool is_vertex_manifold(const SurfaceMesh<Scalar, Index>& mesh, Index vid)
{
    const Index num_corners = mesh.count_num_corners_around_vertex(vid);
    if (num_corners == 0) return true;

    const Index starting_corner = mesh.get_first_corner_around_vertex(vid);
    la_debug_assert(starting_corner < mesh.get_num_corners());
    Index ci = starting_corner;
    Index num_corners_visited = 0;
    do {
        la_debug_assert(mesh.get_corner_vertex(ci) == vid);
        ci = mesh.get_counterclockwise_corner_around_vertex(ci);
        num_corners_visited++;
    } while (ci != invalid<Index>() && ci != starting_corner);

    if (ci == invalid<Index>()) {
        // We hit a boundary or non-manifold edge. Check the other direction.
        ci = starting_corner;
        do {
            la_debug_assert(mesh.get_corner_vertex(ci) == vid);
            ci = mesh.get_clockwise_corner_around_vertex(ci);
            num_corners_visited++;
        } while (ci != invalid<Index>() && ci != starting_corner);
        num_corners_visited--; // The starting corner is counted twice.
    }
    return num_corners_visited == num_corners;
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

    return tbb::parallel_reduce(
        tbb::blocked_range<Index>(0, num_vertices),
        true, ///< initial value of the result.
        [&](const tbb::blocked_range<Index>& r, bool manifold) -> bool {
            if (!manifold) return false;

            for (Index vi = r.begin(); vi != r.end(); vi++) {
                if (!is_vertex_manifold(mesh, vi)) {
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
    mesh.initialize_edges();
    AttributeId id = internal::find_or_create_attribute<uint8_t>(
        mesh,
        options.output_attribute_name,
        Vertex,
        AttributeUsage::Scalar,
        1,
        internal::ResetToDefault::No);
    auto attr = mesh.template ref_attribute<uint8_t>(id).ref_all();

    tbb::parallel_for(
        tbb::blocked_range<Index>(0, mesh.get_num_vertices()),
        [&](const tbb::blocked_range<Index>& r) {
            for (Index vi = r.begin(); vi != r.end(); vi++) {
                attr[vi] = is_vertex_manifold(mesh, vi);
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

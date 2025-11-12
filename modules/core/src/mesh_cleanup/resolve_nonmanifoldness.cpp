/*
 * Copyright 2024 Adobe. All rights reserved.
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
#include <lagrange/foreach_attribute.h>
#include <lagrange/mesh_cleanup/remove_topologically_degenerate_facets.h>
#include <lagrange/mesh_cleanup/resolve_nonmanifoldness.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/concurrent_vector.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
#include <atomic>
#include <string_view>
#include <vector>

namespace lagrange {

template <typename Scalar, typename Index>
void resolve_nonmanifoldness(SurfaceMesh<Scalar, Index>& mesh)
{
    remove_topologically_degenerate_facets(mesh);
    mesh.initialize_edges();

    const Index orig_num_vertices = mesh.get_num_vertices();
    std::vector<Index> corner_map(mesh.get_num_corners(), invalid<Index>());
    tbb::concurrent_vector<Index> vertex_map(orig_num_vertices);
    std::iota(vertex_map.begin(), vertex_map.end(), 0);
    tbb::parallel_for(Index(0), orig_num_vertices, [&](Index vi) {
        Index neighborhood_count = 0;

        mesh.foreach_corner_around_vertex(vi, [&](Index ci) {
            la_debug_assert(mesh.get_corner_vertex(ci) == vi);
            if (corner_map[ci] != invalid<Index>()) return;
            Index vertex_id = invalid<Index>();
            if (neighborhood_count > 0) {
                auto itr = vertex_map.grow_by(1, vi);
                vertex_id = static_cast<Index>(itr - vertex_map.begin());
            } else {
                vertex_id = vi;
            }
            la_debug_assert(vertex_id != invalid<Index>());

            // Traverse corners counterclockwise
            Index cj = ci;
            do {
                if (mesh.get_corner_vertex(cj) != vi) {
                    // Facets are inconsistently oriented
                    break;
                }
                corner_map[cj] = vertex_id;
                cj = mesh.get_counterclockwise_corner_around_vertex(cj);

            } while (cj != ci && cj != invalid<Index>());

            // Traverse corners clockwise
            cj = ci;
            do {
                if (mesh.get_corner_vertex(cj) != vi) {
                    // Facets are inconsistently oriented
                    break;
                }
                corner_map[cj] = vertex_id;
                cj = mesh.get_clockwise_corner_around_vertex(cj);
            } while (cj != ci && cj != invalid<Index>());

            neighborhood_count++;
        });
    });
    Index num_vertices = static_cast<Index>(vertex_map.size());
    if (num_vertices == orig_num_vertices) return;

    // Reset connectivity information.
    seq_foreach_named_attribute_read<AttributeElement::Edge>(
        mesh,
        [&](std::string_view name, [[maybe_unused]] auto&& attr) {
            if (mesh.attr_name_is_reserved(name)) return;
            logger().warn(
                "Edge attribute '{}' will be dropped by `resolve_vertex_nonmanifoldness`",
                name);
        });
    mesh.clear_edges();

    // Add new vertices
    mesh.add_vertices(num_vertices - orig_num_vertices);
    for (Index vi = orig_num_vertices; vi < num_vertices; vi++) {
        const auto p_old = mesh.get_position(vertex_map[vi]);
        auto p_new = mesh.ref_position(vi);
        std::copy(p_old.begin(), p_old.end(), p_new.begin());
    }

    // Update facets
    auto corner_to_vertex = mesh.ref_corner_to_vertex().ref_all();
    la_debug_assert(corner_to_vertex.size() == corner_map.size());
    std::copy(corner_map.begin(), corner_map.end(), corner_to_vertex.begin());

    // Remap vertex attributes
    par_foreach_named_attribute_write<AttributeElement::Vertex>(
        mesh,
        [&](std::string_view name, auto&& attr) {
            if (mesh.attr_name_is_reserved(name)) return;
            for (Index vi = orig_num_vertices; vi < num_vertices; vi++) {
                auto value_new = attr.ref_row(vi);
                auto value_old = attr.get_row(vertex_map[vi]);
                std::copy(value_old.begin(), value_old.end(), value_new.begin());
            }
        });

    // Facet/corner/index/value attributes are unchanged.
}

#define LA_X_resolve_nonmanifoldness(_, Scalar, Index) \
    template LA_CORE_API void resolve_nonmanifoldness<Scalar, Index>(SurfaceMesh<Scalar, Index>&);
LA_SURFACE_MESH_X(resolve_nonmanifoldness, 0)
} // namespace lagrange

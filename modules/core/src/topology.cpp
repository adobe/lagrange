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

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/topology.h>
#include <lagrange/utils/chain_edges.h>
#include <lagrange/utils/invalid.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/blocked_range.h>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_reduce.h>
#include <lagrange/utils/warnon.h>
// clang-format on

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

template <typename Scalar, typename Index>
bool is_vertex_manifold(const SurfaceMesh<Scalar, Index>& mesh)
{
    if (!mesh.has_edges()) {
        auto mesh_copy = mesh;
        mesh_copy.initialize_edges();
        return is_vertex_manifold(mesh_copy);
    }

    const auto num_vertices = mesh.get_num_vertices();
    tbb::enumerable_thread_specific<std::vector<Index>> thread_rim_edges;

    auto is_vertex_manifold = [&](Index vid) {
        auto& rim_edges = thread_rim_edges.local();
        rim_edges.clear();
        rim_edges.reserve(mesh.count_num_corners_around_vertex(vid) * 2);

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
                rim_edges.push_back(vj);
                rim_edges.push_back(vk);
            }
        }

        const auto result = chain_directed_edges<Index>({rim_edges.data(), rim_edges.size()});
        return (result.loops.size() + result.chains.size() == 1);
    };

    return tbb::parallel_reduce(
        tbb::blocked_range<Index>(0, num_vertices),
        true, ///< initial value of the result.
        [&](const tbb::blocked_range<Index>& r, bool manifold) -> bool {
            if (!manifold) return false;

            for (Index vi = r.begin(); vi != r.end(); vi++) {
                if (!is_vertex_manifold(vi)) {
                    return false;
                }
            }
            return true;
        },
        [](bool r1, bool r2) -> bool { return r1 && r2; });
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

#define LA_X_topology(_, Scalar, Index)                                                      \
    template int compute_euler<Scalar, Index>(const SurfaceMesh<Scalar, Index>& mesh);       \
    template bool is_vertex_manifold<Scalar, Index>(const SurfaceMesh<Scalar, Index>& mesh); \
    template bool is_edge_manifold<Scalar, Index>(const SurfaceMesh<Scalar, Index>& mesh);
LA_SURFACE_MESH_X(topology, 0)

} // namespace lagrange

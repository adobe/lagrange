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

#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/mesh_cleanup/remove_isolated_vertices.h>
#include <lagrange/mesh_cleanup/remove_short_edges.h>
#include <lagrange/mesh_cleanup/remove_topologically_degenerate_facets.h>
#include <lagrange/remap_vertices.h>
#include <lagrange/utils/DisjointSets.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/views.h>

#include <vector>

namespace lagrange {

template <typename Scalar, typename Index>
void remove_short_edges(SurfaceMesh<Scalar, Index>& mesh, Scalar threshold)
{
    DisjointSets<Index> clusters;
    std::vector<Index> vertex_map;
    while (true) {
        const Index num_vertices = mesh.get_num_vertices();
        const Index num_facets = mesh.get_num_facets();
        auto vertices = vertex_view(mesh);
        auto corner_vertex = mesh.get_corner_to_vertex().get_all();

        clusters.init(num_vertices);
        bool has_short_edges = false;
        for (Index fi = 0; fi < num_facets; fi++) {
            const Index c_begin = mesh.get_facet_corner_begin(fi);
            const Index c_end = mesh.get_facet_corner_end(fi);
            for (Index ci = c_begin; ci < c_end; ci++) {
                Index cj = (ci + 1) == c_end ? c_begin : ci + 1;
                auto v0 = corner_vertex[ci];
                auto v1 = corner_vertex[cj];
                if (v0 == v1) continue;

                Scalar l = (vertices.row(v0) - vertices.row(v1)).norm();
                if (l <= threshold) {
                    if (clusters.find(v0) == v0 && clusters.find(v1) == v1) {
                        has_short_edges = true;
                        clusters.merge(v0, v1);
                    }
                }
            }
        }

        if (!has_short_edges) break;

        vertex_map.resize(num_vertices);
        clusters.extract_disjoint_set_indices(vertex_map);
        remap_vertices(mesh, {vertex_map.data(), vertex_map.size()});
    }
    remove_topologically_degenerate_facets(mesh);
    remove_isolated_vertices(mesh);
}

#define LA_X_remove_short_edges(_, Scalar, Index)                \
    template LA_CORE_API void remove_short_edges<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                             \
        Scalar);
LA_SURFACE_MESH_X(remove_short_edges, 0)

} // namespace lagrange

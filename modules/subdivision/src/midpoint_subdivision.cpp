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
#include <lagrange/subdivision/api.h>
#include <lagrange/subdivision/midpoint_subdivision.h>

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/eigen_convert.h>
#include <lagrange/views.h>

namespace lagrange::subdivision {

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> midpoint_subdivision(const SurfaceMesh<Scalar, Index>& mesh_)
{
    la_runtime_assert(mesh_.is_triangle_mesh(), "Only triangle meshes are supported");

    auto mesh = mesh_;
    mesh.initialize_edges();
    const auto nv = mesh.get_num_vertices();
    const auto ne = mesh.get_num_edges();
    const auto nf = mesh.get_num_facets();

    SurfaceMesh<Scalar, Index> subdivided_mesh(mesh.get_dimension());

    subdivided_mesh.add_vertices(nv + ne);
    subdivided_mesh.add_triangles(nf * 4);
    auto V = vertex_ref(subdivided_mesh);
    auto F = facet_ref(subdivided_mesh);

    V.topRows(nv) = vertex_view(mesh);
    for (Index e = 0; e < ne; ++e) {
        auto v = mesh.get_edge_vertices(e);
        V.row(nv + e) = Scalar(0.5) * (V.row(v[0]) + V.row(v[1]));
    }
    for (Index f = 0; f < nf; ++f) {
        auto facet = mesh.get_facet_vertices(f);
        const Index v0 = facet[0];
        const Index v1 = facet[1];
        const Index v2 = facet[2];
        const Index e0 = nv + mesh.get_edge(f, 0);
        const Index e1 = nv + mesh.get_edge(f, 1);
        const Index e2 = nv + mesh.get_edge(f, 2);
        F.row(4 * f + 0) << v0, e0, e2;
        F.row(4 * f + 1) << v1, e1, e0;
        F.row(4 * f + 2) << v2, e2, e1;
        F.row(4 * f + 3) << e0, e1, e2;
    }

    return subdivided_mesh;
}

#define LA_X_midpoint_subdivision(_, Scalar, Index)                              \
    template LA_SUBDIVISION_API SurfaceMesh<Scalar, Index> midpoint_subdivision( \
        const SurfaceMesh<Scalar, Index>& mesh);
LA_SURFACE_MESH_X(midpoint_subdivision, 0)

} // namespace lagrange::subdivision

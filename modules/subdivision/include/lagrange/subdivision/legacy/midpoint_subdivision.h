/*
 * Copyright 2020 Adobe. All rights reserved.
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

#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/create_mesh.h>
#include <lagrange/legacy/inline.h>

namespace lagrange {
namespace subdivision {
LAGRANGE_LEGACY_INLINE
namespace legacy {

///
/// Performs one step of midpoint subdivision for triangle meshes.
///
/// @note          This function currently does not remap any mesh attribute.
///
/// @param[in,out] mesh      Input mesh to subdivide. Modified to compute edge data.
///
/// @tparam        MeshType  Mesh type.
///
/// @return        Subdivided mesh.
///
template <
    typename MeshType,
    std::enable_if_t<lagrange::MeshTraitHelper::is_mesh<MeshType>::value>* = nullptr>
std::unique_ptr<MeshType> midpoint_subdivision(MeshType& mesh)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    la_runtime_assert(mesh.get_vertex_per_facet() == 3, "Only triangle meshes are supported");

    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;

    mesh.initialize_edge_data();
    const auto nv = mesh.get_num_vertices();
    const auto ne = mesh.get_num_edges();
    const auto nf = mesh.get_num_facets();

    VertexArray V(nv + ne, mesh.get_dim());
    FacetArray F(nf * 4, 3);
    V.topRows(nv) = mesh.get_vertices();
    for (Index e = 0; e < ne; ++e) {
        auto v = mesh.get_edge_vertices(e);
        V.row(nv + e) =
            Scalar(0.5) * (mesh.get_vertices().row(v[0]) + mesh.get_vertices().row(v[1]));
    }
    for (Index f = 0; f < mesh.get_num_facets(); ++f) {
        const Index v0 = mesh.get_facets()(f, 0);
        const Index v1 = mesh.get_facets()(f, 1);
        const Index v2 = mesh.get_facets()(f, 2);
        const Index e0 = nv + mesh.get_edge(f, 0);
        const Index e1 = nv + mesh.get_edge(f, 1);
        const Index e2 = nv + mesh.get_edge(f, 2);
        F.row(4 * f + 0) << v0, e0, e2;
        F.row(4 * f + 1) << v1, e1, e0;
        F.row(4 * f + 2) << v2, e2, e1;
        F.row(4 * f + 3) << e0, e1, e2;
    }

    return lagrange::create_mesh(V, F);
}

} // namespace legacy
} // namespace subdivision
} // namespace lagrange

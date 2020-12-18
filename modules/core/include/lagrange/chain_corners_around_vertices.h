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
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <lagrange/utils/range.h>
#include <lagrange/utils/safe_cast.h>

#include <Eigen/Core>
////////////////////////////////////////////////////////////////////////////////

namespace lagrange {

///
/// Chains facet corners around vertices of a mesh. The mesh is assumed have polygonal faces of
/// constant degree k. There are #C = #F * k facet corners in this mesh.
///
/// @param[in]  num_vertices               Number of vertices in the mesh. This information is
///                                        necessary since there may be isolated vertices, and thus
///                                        computing it from the facet array may be misleading.
/// @param[in]  facets                     #F x k array of facet indices.
/// @param[out] vertex_to_corner           #E x 1 array of first facet corner in the chain starting
///                                        from a given vertex.
/// @param[out] next_corner_around_vertex  #C x 1 array of next facet corner in the chain at a given
///                                        facet corner.
///
/// @tparam     DerivedF                   Type of facet array.
/// @tparam     DerivedE                   Type of vertex to corner vector.
/// @tparam     DerivedN                   Type of next corner vector.
///
template <typename DerivedF, typename DerivedE, typename DerivedN>
void chain_corners_around_vertices(
    typename DerivedF::Scalar num_vertices,
    const Eigen::MatrixBase<DerivedF> &facets,
    Eigen::PlainObjectBase<DerivedE> &vertex_to_corner,
    Eigen::PlainObjectBase<DerivedN> &next_corner_around_vertex)
{
    using Index = typename DerivedF::Scalar;

    Index num_corners = safe_cast<Index>(facets.rows() * facets.cols());

    // Chain corners around vertices and edges
    vertex_to_corner.resize(num_vertices);
    vertex_to_corner.setConstant(INVALID<Index>());
    next_corner_around_vertex.resize(num_corners);
    next_corner_around_vertex.setConstant(INVALID<Index>());
    for (auto f : range(facets.rows())) {
        for (auto lv : range(facets.cols())) {
            Index c = (Index)(f * facets.cols() + lv);
            Index v = facets(f, lv);
            next_corner_around_vertex[c] = vertex_to_corner[v];
            vertex_to_corner[v] = c;
        }
    }
}

} // namespace lagrange

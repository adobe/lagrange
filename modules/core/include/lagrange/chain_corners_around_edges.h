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

#include <Eigen/Core>
////////////////////////////////////////////////////////////////////////////////

namespace lagrange {

///
/// Chains facet corners around edges of a mesh. The mesh is assumed have polygonal faces of
/// constant degree k. There are #C = #F * k facet corners in this mesh.
///
/// @param[in]  facets                   #F x k array of facet indices.
/// @param[in]  corner_to_edge           #C x 1 array mapping facet corners to edge indices.
/// @param[out] edge_to_corner           #E x 1 array of first facet corner in the chain starting
///                                      from a given edge.
/// @param[out] next_corner_around_edge  #C x 1 array of next facet corner in the chain at a given
///                                      facet corner.
///
/// @tparam     DerivedF                 Type of facet array.
/// @tparam     DerivedC                 Type of corner to edge vector.
/// @tparam     DerivedE                 Type of edge to corner vector.
/// @tparam     DerivedN                 Type of next corner vector.
///
template <typename DerivedF, typename DerivedC, typename DerivedE, typename DerivedN>
void chain_corners_around_edges(
    const Eigen::MatrixBase<DerivedF>& facets,
    const Eigen::MatrixBase<DerivedC>& corner_to_edge,
    Eigen::PlainObjectBase<DerivedE>& edge_to_corner,
    Eigen::PlainObjectBase<DerivedN>& next_corner_around_edge)
{
    using Index = typename DerivedF::Scalar;

    Index num_edges = (corner_to_edge.size() ? corner_to_edge.maxCoeff() + 1 : 0);
    Index num_corners = (Index)(facets.rows() * facets.cols());

    // Chain corners around vertices and edges
    edge_to_corner.resize(num_edges);
    edge_to_corner.setConstant(INVALID<Index>());
    next_corner_around_edge.resize(num_corners);
    next_corner_around_edge.setConstant(INVALID<Index>());
    for (auto f : range(facets.rows())) {
        for (auto lv : range(facets.cols())) {
            Index c = (Index)(f * facets.cols() + lv);
            Index e = corner_to_edge(c);
            next_corner_around_edge[c] = edge_to_corner[e];
            edge_to_corner[e] = c;
        }
    }
}

} // namespace lagrange

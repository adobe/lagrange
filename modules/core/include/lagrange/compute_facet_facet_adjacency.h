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
#include <lagrange/utils/AdjacencyList.h>

namespace lagrange {

///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities.
///
/// @{

///
/// Compute facet-facet adjacency information based on shared edges.
///
/// Two facets are considered adjacent if they share an edge.  For non-manifold edges with 3 or more
/// incident facets, a complete clique is formed (every pair of facets around the edge is adjacent).
///
/// @note       If two facets share multiple edges, the neighboring facet will appear in the
///             adjacency list once for each time the shared edge is referenced by its incident
///             facets (i.e., neighbors are not deduplicated).
///
/// @note       This function calls @c initialize_edges() if edges have not been initialized yet,
///             which mutates the mesh.
///
/// @param      mesh    The input mesh (edges will be initialized if needed).
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
/// @return     The facet-facet adjacency list.
///
template <typename Scalar, typename Index>
AdjacencyList<Index> compute_facet_facet_adjacency(SurfaceMesh<Scalar, Index>& mesh);

/// @}

} // namespace lagrange

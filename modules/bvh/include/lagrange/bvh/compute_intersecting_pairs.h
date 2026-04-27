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

namespace lagrange::bvh {

///
/// @defgroup   group-bvh-intersecting-pairs Intersecting Pairs
/// @ingroup    group-bvh
///
/// Compute intersecting facet pairs in a mesh using BVH acceleration.
///
/// @{

///
/// Compute all pairs of intersecting facets in a mesh using an AABB tree for acceleration.
///
/// Detects facet pairs whose interiors overlap using exact geometric predicates. All facet
/// pairs (including vertex- and edge-adjacent ones) are tested geometrically. Contacts at
/// shared vertices or edges do not count as intersections; only interior overlaps are reported.
///
/// @param[in]  mesh  The input mesh. Must be a triangle mesh (only triangular facets).
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
/// @return     An AdjacencyList representing the intersection graph. For each facet i,
///             get_neighbors(i) returns all facets whose interior overlaps the interior of i.
///
/// @throws     std::runtime_error if the mesh is not a triangle mesh or not 3D.
///
/// @note       Uses exact predicates (orient3D, orient2D) with include_boundary=false.
///             Boundary contacts (shared vertices or edges) are correctly excluded.
///
/// @see        triangle_triangle_intersection
///
template <typename Scalar, typename Index>
AdjacencyList<Index> compute_intersecting_pairs(const SurfaceMesh<Scalar, Index>& mesh);

/// @}

} // namespace lagrange::bvh

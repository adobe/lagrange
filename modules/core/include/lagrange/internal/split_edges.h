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
#pragma once

#include <lagrange/SurfaceMesh.h>
#include <lagrange/utils/function_ref.h>
#include <lagrange/utils/span.h>

namespace lagrange::internal {

///
/// Split edges based on the input split points, retriangulating the affected facets accordingly.
///
/// @param mesh               Input mesh which will be updated in place. All potential splits should
///                           be represented as vertices in the mesh. The mesh will be updated in
///                           place.
/// @param get_edge_split_pts A function that returns the split points for each edge. The function
///                           takes an edge ID as input and returns a span of vertex indices
///                           representing the split pts.
/// @param active_facet       A function that takes a facet ID as input and returns true if the facet
///                           is active. Only active facets will be split.
///
/// @note The original facets that got split will remain in the mesh. The callers are responsible
/// for what to do with them (e.g., remove them if needed).
///
/// @note Facet, corner and indexed attributes are propagated to the newly created facets/corners.
///
/// @return A vector of original facet IDs that were split.
///
template <typename Scalar, typename Index>
std::vector<Index> split_edges(
    SurfaceMesh<Scalar, Index>& mesh,
    function_ref<span<Index>(Index)> get_edge_split_pts,
    function_ref<bool(Index)> active_facet);


///
/// Split edges based on the input split points *without* retriangulating the affected facets (in
/// contrast to `split_edges`, which does retriangulate).
///
/// Each original facet is left untouched in place. For every affected facet, a single new facet is
/// appended whose corner chain has the edge split points inserted into it, so the facet grows by
/// one corner per split point (e.g. a triangle with one split edge becomes a quad). The new facets
/// are appended after all existing facets, so their facet IDs are contiguous and all `>=` the facet
/// count at call time; callers can use this to identify the newly created facets (e.g. to
/// selectively triangulate only them). The mesh may become hybrid as a result.
///
/// @param mesh               Input mesh which will be updated in place. All potential splits should
///                           be represented as vertices in the mesh. The mesh will be updated in
///                           place.
/// @param get_edge_split_pts A function that returns the split points for each edge. The function
///                           takes an edge ID as input and returns a span of vertex indices
///                           representing the split pts.
/// @param active_facet       A function that takes a facet ID as input and returns true if the facet
///                           is active. Only active facets will be affected.
///
/// @note The original facets that got split remain in the mesh unmodified. The callers are
/// responsible for what to do with them (e.g., remove them if needed).
///
/// @note Facet, corner and indexed attributes are propagated to the newly created facets/corners.
/// Values at inserted split points are linearly interpolated along the split edge, so the split
/// edge must not be geometrically degenerate (its endpoints must have distinct positions).
///
/// @return A vector of original facet IDs that were split.
///
template <typename Scalar, typename Index>
std::vector<Index> split_edges_only(
    SurfaceMesh<Scalar, Index>& mesh,
    function_ref<span<Index>(Index)> get_edge_split_pts,
    function_ref<bool(Index)> active_facet);

} // namespace lagrange::internal

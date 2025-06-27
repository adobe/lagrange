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

/**
 * Split edges based on the input split points.
 *
 * @param mesh               Input mesh which will be updated in place. All potential splits should
 *                           be represented as vertices in the mesh. The mesh will be updated in
 *                           place.
 * @param get_edge_split_pts A function that returns the split points for each edge. The function
 *                           takes an edge ID as input and returns a span of vertex indices
 *                           representing the split pts.
 * @param active_facet       A function that takes a facet ID as input and returns true if the facet
 *                           is active. Only active facets will be split.
 *
 * @return A vector of original facet IDs that were split.
 */
template <typename Scalar, typename Index>
std::vector<Index> split_edges(
    SurfaceMesh<Scalar, Index>& mesh,
    function_ref<span<Index>(Index)> get_edge_split_pts,
    function_ref<bool(Index)> active_facet);

} // namespace lagrange::internal

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

#include <lagrange/raycasting/api.h>
#include <lagrange/utils/span.h>

#include <cstdint>
#include <utility>
#include <vector>

namespace lagrange::raycasting {

/// @cond LA_INTERNAL_DOCS

/// Compute a binary facet keep mask from per-facet scores and adjacency edges.
///
/// Each score supplies the source capacity for its facet, while @p threshold supplies its sink
/// capacity. Every edge in @p adjacency_edges applies the symmetric @p smoothness penalty when
/// its endpoints receive different labels. The returned mask contains 1 for facets on the source
/// (keep) side of the cut and 0 for facets on the sink (cull) side.
///
/// With @p smoothness equal to zero, the result is computed directly: a facet is kept iff its
/// score is at least @p threshold. No graph is constructed in that case.
///
/// @param[in] scores           Non-negative finite per-facet keep scores.
/// @param[in] threshold        Non-negative finite cull threshold.
/// @param[in] adjacency_edges  Undirected facet-adjacency pairs indexed into @p scores.
/// @param[in] smoothness       Non-negative finite penalty for disagreeing adjacent labels.
///
/// @return A keep mask with the same length as @p scores.
///
LA_RAYCASTING_API std::vector<uint8_t> compute_graphcut_mask(
    span<const double> scores,
    double threshold,
    span<const std::pair<uint64_t, uint64_t>> adjacency_edges,
    double smoothness);

/// @endcond

} // namespace lagrange::raycasting

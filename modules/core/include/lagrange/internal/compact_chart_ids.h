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

#include <lagrange/utils/span.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_sort.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
#include <utility>
#include <vector>

namespace lagrange::internal {

///
/// Compact chart ids to a dense [0, N) range so per-chart storage is bounded by the number of
/// distinct charts, regardless of how sparse user-supplied chart ids may be.
///
/// @param[in]  chart_ids  Per-facet chart ids (may be sparse).
///
/// @tparam     Index      Index type.
///
/// @return     Pair of (dense per-facet chart ids, number of distinct charts).
///
template <typename Index>
std::pair<std::vector<Index>, Index> compact_chart_ids(span<const Index> chart_ids)
{
    std::vector<Index> sorted_ids(chart_ids.begin(), chart_ids.end());
    tbb::parallel_sort(sorted_ids.begin(), sorted_ids.end());
    sorted_ids.erase(std::unique(sorted_ids.begin(), sorted_ids.end()), sorted_ids.end());
    const Index num_charts = static_cast<Index>(sorted_ids.size());

    std::vector<Index> dense_chart_ids(chart_ids.size());
    tbb::parallel_for(Index(0), static_cast<Index>(chart_ids.size()), [&](Index f) {
        dense_chart_ids[f] = static_cast<Index>(
            std::lower_bound(sorted_ids.begin(), sorted_ids.end(), chart_ids[f]) -
            sorted_ids.begin());
    });
    return {std::move(dense_chart_ids), num_charts};
}

} // namespace lagrange::internal

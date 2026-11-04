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

#include <lagrange/raycasting/remove_occluded_facets.h>

namespace lagrange::raycasting {

// Private helpers retain exported linkage for white-box tests in shared-library builds.
LA_RAYCASTING_API void validate_view_error_options(const OccludedFacetViewErrorOptions& options);

LA_RAYCASTING_API OccludedFacetViewErrorResult solve_graphcut_with_view_error(
    span<const double> scores,
    span<const std::pair<uint64_t, uint64_t>> adjacency_edges,
    RenderErrorEvaluator evaluate,
    const OccludedFacetViewErrorOptions& options = {},
    uint64_t num_worst_views = 3);

} // namespace lagrange::raycasting

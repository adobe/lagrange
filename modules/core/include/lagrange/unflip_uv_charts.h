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

#include <string_view>

namespace lagrange {

///
/// @addtogroup group-surfacemesh-utils
/// @{
///

struct UnflipUVChartsOptions
{
    /// Input UV attribute name.
    /// If empty, the first indexed UV attribute will be used. The UV attribute must be indexed.
    std::string_view uv_attribute_name = "";

    /// Optional per-facet chart id attribute name.
    /// If empty, chart ids are computed automatically using edge connectivity on the UV mesh.
    std::string_view chart_id_attribute_name = "";
};

/**
 * Mirror the UV positions of every UV vertex in any chart that is "flipped".
 *
 * A chart is considered flipped when either:
 * - its total signed UV area (with each facet's `|area|` signed by the per-facet flip bit from
 *   @ref compute_uv_orientation) is negative, OR
 * - the chart contains at least one flipped triangle, and no positively-oriented triangles (i.e.
 *   triangles are either flipped or degenerate).
 *
 * The second predicate catches charts whose floating-point signed area happens to be non-negative
 * because of nearly-degenerate triangles, but whose every non-degenerate triangle is still oriented
 * incorrectly.
 *
 * For each flipped chart, the U coordinate of every UV vertex referenced by that chart's facets is
 * negated in place (`u <- -u`). This inverts each triangle's orientation in UV space without
 * touching the mesh's corner ordering or topology. Assumes UV vertices are not shared across
 * charts (the typical case for charts produced by @ref disconnect_uv_charts or
 * @ref compute_uv_charts on indexed UV attributes). This fixes globally mirrored charts, but it
 * does not repair charts with locally inconsistent per-facet winding.
 *
 * @param      mesh     Input triangle mesh. The UV attribute must be indexed.
 * @param      options  Options to control chart unflipping.
 *
 * @tparam     Scalar   Mesh scalar type.
 * @tparam     Index    Mesh index type.
 *
 * @return     The number of charts that were unflipped.
 *
 * @see        @ref UnflipUVChartsOptions, @ref compute_uv_orientation
 */
template <typename Scalar, typename Index>
size_t unflip_uv_charts(
    SurfaceMesh<Scalar, Index>& mesh,
    const UnflipUVChartsOptions& options = {});

/// @}

} // namespace lagrange

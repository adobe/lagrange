/*
 * Copyright 2022 Adobe. All rights reserved.
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

#include <lagrange/ExactPredicates.h>
#include <lagrange/legacy/inline.h>
#include <lagrange/utils/range.h>

#include <Eigen/Core>

namespace lagrange {
LAGRANGE_LEGACY_INLINE
namespace legacy {

/**
 * Check for flipped uv facets.
 *
 * @param[in] uv           The UV coordinates.
 * @param[in] uv_indices   The UV indices.
 * @param[in,out] flipped  An optional buffer to store an indicator flag per
 *                         facet.
 *
 * @returns The number of facets with flipped uvs.
 */
template <typename UVArray, typename UVIndices, typename T = void>
size_t check_flipped_uv(
    const Eigen::PlainObjectBase<UVArray>& uv,
    const Eigen::PlainObjectBase<UVIndices>& uv_indices,
    T* flipped = nullptr)
{
    const auto num_facets = uv_indices.rows();
    const auto predicates = ExactPredicates::create("shewchuk");
    size_t num_flipped = 0;
    double uv0[2], uv1[2], uv2[2];
    for (auto i : range(num_facets)) {
        // Explicit cast needed for orientation check.
        uv0[0] = static_cast<double>(uv(uv_indices(i, 0), 0));
        uv0[1] = static_cast<double>(uv(uv_indices(i, 0), 1));
        uv1[0] = static_cast<double>(uv(uv_indices(i, 1), 0));
        uv1[1] = static_cast<double>(uv(uv_indices(i, 1), 1));
        uv2[0] = static_cast<double>(uv(uv_indices(i, 2), 0));
        uv2[1] = static_cast<double>(uv(uv_indices(i, 2), 1));
        auto r = predicates->orient2D(uv0, uv1, uv2);
        if (r <= 0) num_flipped++;
        if (flipped) flipped[i] = r <= 0;
    }

    return num_flipped;
}

} // namespace legacy
} // namespace lagrange

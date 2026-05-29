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

#include <cstddef>
#include <string_view>

namespace lagrange {

///
/// @addtogroup group-surfacemesh-utils
/// @{
///

struct UVOrientationOptions
{
    /// Input UV attribute name.
    /// If empty, the first vertex/indexed UV attribute will be used.
    std::string_view uv_attribute_name = "";

    /// Output per-facet attribute name. Stores the raw `orient2D` result as `int8_t`:
    /// `+1` for positively oriented (CCW), `0` for degenerate, `-1` for negatively oriented (CW).
    std::string_view output_attribute_name = "@uv_orientation";
};

/// Per-facet orientation counts returned by @ref compute_uv_orientation.
struct UVOrientationCount
{
    size_t positive = 0; ///< Number of CCW (positively oriented) facets.
    size_t degenerate = 0; ///< Number of degenerate (zero-area) facets.
    size_t negative = 0; ///< Number of CW (negatively oriented / flipped) facets.
};

/**
 * Compute a per-facet orientation attribute using Shewchuk's exact `orient2D` predicate.
 *
 * Each facet is assigned an `int8_t` value matching the raw predicate result:
 * - `+1`: positively oriented (CCW) in UV space.
 * - `0`: degenerate (UV vertices are collinear).
 * - `-1`: negatively oriented (CW / flipped) in UV space.
 *
 * @param      mesh     Input mesh. Must be a triangle mesh.
 * @param      options  Options to control orientation computation.
 *
 * @tparam     Scalar   Mesh scalar type.
 * @tparam     Index    Mesh index type.
 *
 * @return     Counts of positively oriented, degenerate, and negatively oriented facets.
 *
 * @see        @ref UVOrientationOptions
 */
template <typename Scalar, typename Index>
UVOrientationCount compute_uv_orientation(
    SurfaceMesh<Scalar, Index>& mesh,
    const UVOrientationOptions& options = {});

/// @}

} // namespace lagrange

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
#include <lagrange/internal/constants.h>

#include <cstddef>
#include <string_view>

namespace lagrange {

///
/// @addtogroup group-surfacemesh-cleanup
/// @{
///

///
/// Options for the `split_obtuse_triangles` function.
///
struct SplitObtuseTrianglesOptions
{
    /// Maximum allowed interior angle in radians. Triangles with any interior angle strictly
    /// greater than this threshold will be split. Default is `pi/2`, so any triangle with an
    /// angle larger than 90 degrees is considered obtuse.
    float max_angle = static_cast<float>(lagrange::internal::pi_2);

    /// Maximum number of split passes. Each pass projects the obtuse vertex onto the opposite
    /// edge and splits both incident triangles. Use `0` to iterate until convergence.
    size_t max_iterations = 5;

    /// Optional facet attribute (`uint8_t`) restricting which facets are considered obtuse
    /// candidates. If empty, every facet is checked.
    std::string_view active_region_attribute = "";
};

///
/// Iteratively split obtuse triangles by splitting their longest edge at the projection of the
/// opposite (obtuse) vertex. After each pass, both triangles incident to a split edge are
/// re-tessellated. Vertex attributes are linearly interpolated along the split edge.
///
/// @param[in,out] mesh    Input triangle mesh, updated in place.
/// @param[in]     options Optional settings.
///
/// @return Total number of triangle splits performed across all iterations (a triangle re-split in
///         a later iteration is counted again).
///
template <typename Scalar, typename Index>
size_t split_obtuse_triangles(
    SurfaceMesh<Scalar, Index>& mesh,
    SplitObtuseTrianglesOptions options = {});

/// @}

} // namespace lagrange

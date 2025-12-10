/*
 * Copyright 2019 Adobe. All rights reserved.
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

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/primitive/legacy/generate_rounded_cube.h>
#endif

#include <lagrange/SurfaceMesh.h>
#include <lagrange/primitive/PrimitiveOptions.h>

namespace lagrange::primitive {

/// @addtogroup module-primitive
/// @{

/**
 * Options for generating a rounded cube mesh.
 *
 * This structure contains all the parameters needed to generate a rounded cube mesh,
 * including dimensions, tessellation parameters, and beveling options.
 * The rounded cube can have smooth rounded edges controlled by the bevel radius
 * and number of bevel segments.
 */
struct RoundedCubeOptions : public PrimitiveOptions
{
    using Scalar = PrimitiveOptions::Scalar;
    using Index = size_t;

    /// Width of the cube along the X-axis
    Scalar width = 1;

    /// Height of the cube along the Y-axis
    Scalar height = 1;

    /// Depth of the cube along the Z-axis
    Scalar depth = 1;

    /// Number of segments along the width (X-axis)
    Index width_segments = 1;

    /// Number of segments along the height (Y-axis)
    Index height_segments = 1;

    /// Number of segments along the depth (Z-axis)
    Index depth_segments = 1;

    /// Radius of the bevel/rounding applied to cube edges
    /// A value of 0 creates a regular cube with sharp edges
    Scalar bevel_radius = 0;

    /// Number of segments used to approximate each rounded edge
    /// Higher values create smoother rounded edges but increase vertex count
    Index bevel_segments = 8;

    /**
     * Clamps all parameters to valid ranges.
     *
     * This method ensures that:
     * - All dimensions (width, height, depth) are non-negative
     * - All segment counts are at least 1
     * - Bevel radius is non-negative and doesn't exceed half the smallest dimension
     * - Bevel segments is at least 1 if beveling is enabled, or 0 if disabled
     */
    void project_to_valid_range()
    {
        width = std::max(width, Scalar(0));
        height = std::max(height, Scalar(0));
        depth = std::max(depth, Scalar(0));

        width_segments = std::max(width_segments, Index(1));
        height_segments = std::max(height_segments, Index(1));
        depth_segments = std::max(depth_segments, Index(1));

        bevel_radius = std::max(bevel_radius, Scalar(0));
        Scalar max_acceptable_radius = (std::min(std::min(width, height), depth)) / 2;
        bevel_radius = std::min(bevel_radius, max_acceptable_radius);

        if (bevel_radius > this->epsilon) {
            bevel_segments = std::max(bevel_segments, Index(1));
        } else {
            bevel_segments = 0;
        }
    }
};

/**
 * Generate a rounded cube mesh.
 *
 * Creates a cube mesh with optionally rounded edges. The cube is centered at the origin
 * and extends from -width/2 to +width/2 along the X-axis, -height/2 to +height/2 along
 * the Y-axis, and -depth/2 to +depth/2 along the Z-axis.
 *
 * When bevel_radius > 0, the edges and corners of the cube are rounded using cylindrical
 * and spherical patches respectively. The smoothness of the rounding is controlled by
 * the bevel_segments parameter.
 *
 * @tparam Scalar  The scalar type for vertex coordinates (e.g., float, double)
 * @tparam Index   The index type for face indices (e.g., uint32_t, uint64_t)
 *
 * @param setting  Configuration parameters for the rounded cube generation
 *
 * @return A surface mesh representing the rounded cube
 *
 * @note The function automatically calls setting.project_to_valid_range() to ensure
 *       all parameters are within valid bounds before generation.
 */
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_rounded_cube(RoundedCubeOptions setting);

/// @}

} // namespace lagrange::primitive

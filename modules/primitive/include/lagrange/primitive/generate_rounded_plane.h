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
    #include <lagrange/primitive/legacy/generate_rounded_plane.h>
#endif

#include <lagrange/SurfaceMesh.h>
#include <lagrange/primitive/PrimitiveOptions.h>

namespace lagrange::primitive {

/// @addtogroup module-primitive
/// @{

///
/// Options for generating a rounded plane mesh.
///
struct RoundedPlaneOptions : public PrimitiveOptions
{
    using Scalar = PrimitiveOptions::Scalar;
    using Index = size_t;

    /// Width of the plane along the X-axis.
    /// Must be non-negative.
    Scalar width = 1;

    /// Height of the plane along the Z-axis.
    /// Must be non-negative.
    Scalar height = 1;

    /// Radius of the bevel/rounding applied to the plane corners.
    /// A value of 0 creates sharp corners. The bevel radius is automatically
    /// clamped to at most half the minimum of width and height.
    Scalar bevel_radius = 0;

    /// Number of subdivisions along the width (X-axis).
    /// Must be at least 1.
    Index width_segments = 1;

    /// Number of subdivisions along the height (Z-axis).
    /// Must be at least 1.
    Index height_segments = 1;

    /// Number of subdivisions for the bevel/rounded corners.
    /// Higher values create smoother rounded corners. This parameter is
    /// ignored if bevel_radius is 0 or smaller than epsilon.
    /// Must be at least 1 when bevel_radius > epsilon.
    Index bevel_segments = 8;

    /// Unit normal vector for the plane.
    std::array<Scalar, 3> normal = {0, 0, 1};

    ///
    /// Clamps all parameters to valid ranges.
    ///
    /// This method ensures that:
    /// - width and height are non-negative
    /// - bevel_radius is non-negative and at most half the minimum of width and height
    /// - width_segments and height_segments are at least 1
    /// - bevel_segments is at least 1 when bevel_radius > epsilon, or 0 otherwise
    ///
    void project_to_valid_range()
    {
        width = std::max(width, static_cast<Scalar>(0));
        height = std::max(height, static_cast<Scalar>(0));
        bevel_radius = std::min(
            std::max(bevel_radius, static_cast<Scalar>(0)),
            (std::min(width, height)) / static_cast<Scalar>(2));

        width_segments = std::max(width_segments, static_cast<Index>(1));
        height_segments = std::max(height_segments, static_cast<Index>(1));

        if (bevel_radius > this->epsilon) {
            bevel_segments = std::max(bevel_segments, static_cast<Index>(1));
        } else {
            bevel_segments = 0;
        }

        const Scalar l =
            std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
        if (l > this->epsilon) {
            normal[0] /= l;
            normal[1] /= l;
            normal[2] /= l;
        } else {
            // Degenerate normal, reset to default
            normal = {0, 0, 1};
        }
    }
};

///
/// Generate a rounded plane mesh.
///
/// The mesh will be centered at `setting.center`, with its normal pointing along `setting.normal`.
///
/// The generated mesh includes:
/// - Optional normal vectors (if setting.normal_attribute_name is specified)
/// - Optional UV coordinates (if setting.uv_attribute_name is specified)
/// - Optional semantic labels (if setting.semantic_label_attribute_name is specified)
///
/// @tparam Scalar  Floating-point type for vertex coordinates.
/// @tparam Index   Integer type for vertex and face indices.
///
/// @param settings Configuration options for the rounded plane generation.
///                 The settings are automatically validated and clamped to valid ranges.
///
/// @return A SurfaceMesh containing the generated rounded plane geometry.
///
/// @see @ref RoundedPlaneOptions for detailed parameter descriptions.
///
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_rounded_plane(RoundedPlaneOptions settings);

/// @}

} // namespace lagrange::primitive

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
    #include <lagrange/primitive/legacy/generate_rounded_cone.h>
#endif

#include <lagrange/SurfaceMesh.h>
#include <lagrange/internal/constants.h>
#include <lagrange/primitive/PrimitiveOptions.h>

namespace lagrange::primitive {

/// @addtogroup module-primitive
/// @{

///
/// Options for generating a rounded cone mesh.
///
struct RoundedConeOptions : public PrimitiveOptions
{
    using Scalar = PrimitiveOptions::Scalar;
    using Index = size_t;

    /// Radius of the cone at the top. Set to 0 for a traditional cone.
    Scalar radius_top = 0;

    /// Radius of the cone at the bottom.
    Scalar radius_bottom = 1;

    /// Height of the cone along the Y-axis.
    Scalar height = 1;

    /// Radius of the bevel/rounding applied to the top edge.
    /// A value of 0 creates a sharp edge.
    Scalar bevel_radius_top = 0;

    /// Radius of the bevel/rounding applied to the bottom edge.
    /// A value of 0 creates a sharp edge.
    Scalar bevel_radius_bottom = 0;

    /// Number of radial subdivisions around the cone circumference.
    /// Higher values create smoother circular cross-sections.
    Index radial_sections = 32;

    /// Number of segments used to approximate the top rounded edge.
    /// Only relevant when bevel_radius_top > 0.
    Index bevel_segments_top = 1;

    /// Number of segments used to approximate the bottom rounded edge.
    /// Only relevant when bevel_radius_bottom > 0.
    Index bevel_segments_bottom = 1;

    /// Number of segments along the cone's side surface (height direction).
    Index side_segments = 1;

    /// Number of radial segments on the top cap when radius_top > 0.
    Index top_segments = 1;

    /// Number of radial segments on the bottom cap when radius_bottom > 0.
    Index bottom_segments = 1;

    /// Starting angle for partial cone generation (in radians).
    /// 0 corresponds to the positive X-axis.
    Scalar start_sweep_angle = 0;

    /// Ending angle for partial cone generation (in radians).
    /// Default of 2π creates a full cone.
    Scalar end_sweep_angle = static_cast<Scalar>(2 * lagrange::internal::pi);

    ///
    /// Clamps all parameters to valid ranges.
    ///
    /// This method ensures that:
    /// - All radii (radius_top, radius_bottom) are non-negative
    /// - Height is non-negative
    /// - Bevel radii are non-negative and don't exceed geometric constraints
    /// - All segment counts are at least 1
    ///
    /// The bevel radius constraints are computed based on cone geometry to prevent invalid
    /// configurations where bevels would overlap or exceed the cone dimensions.
    ///
    void project_to_valid_range()
    {
        radius_top = std::max(radius_top, Scalar(0));
        radius_bottom = std::max(radius_bottom, Scalar(0));
        height = std::max(height, Scalar(0));

        auto [max_bevel_top, max_bevel_bottom] = get_max_cone_bevel();
        bevel_radius_top = std::clamp(bevel_radius_top, Scalar(0), max_bevel_top);
        bevel_radius_bottom = std::clamp(bevel_radius_bottom, Scalar(0), max_bevel_bottom);

        radial_sections = std::max(radial_sections, Index(1));
        bevel_segments_top = std::max(bevel_segments_top, Index(1));
        bevel_segments_bottom = std::max(bevel_segments_bottom, Index(1));

        side_segments = std::max(side_segments, Index(1));
        top_segments = std::max(top_segments, Index(1));
        bottom_segments = std::max(bottom_segments, Index(1));
    }

    ///
    /// Computes the maximum allowable bevel radii for the cone geometry.
    ///
    /// This function calculates geometric constraints on bevel radii based on the cone's dimensions
    /// and slope. The maximum bevel radius is limited by both the radius at each end and the cone's
    /// height to prevent geometric inconsistencies.
    ///
    /// @return A pair containing (max_bevel_top, max_bevel_bottom) where:
    ///         - max_bevel_top: Maximum allowable bevel radius for the top edge
    ///         - max_bevel_bottom: Maximum allowable bevel radius for the bottom edge
    ///
    /// @note The calculation considers the cone's slope angle and ensures bevels don't
    ///       exceed half the height or create overlapping rounded regions.
    ///
    std::pair<Scalar, Scalar> get_max_cone_bevel() const
    {
        // angle between the cone slope and the vertical line (0 for cylinders)
        Scalar psi = std::atan2((radius_top - radius_bottom), height);
        Scalar a1 = (Scalar)(lagrange::internal::pi_2 + psi) * .5f;
        Scalar a2 = (Scalar)(lagrange::internal::pi_2 - psi) * .5f;

        Scalar max_bevel_bottom =
            radius_bottom * std::tan(a1); // max bevel against radius and slope
        max_bevel_bottom = std::min(height * .5f, max_bevel_bottom); // also check against height
        max_bevel_bottom = std::max(max_bevel_bottom, 0.f);


        Scalar max_bevel_top =
            radius_top * std::tan(a2); // max bevel against other radius and inverse slope
        max_bevel_top = std::min(height * .5f, max_bevel_top); // also check against height
        max_bevel_top = std::max(max_bevel_top, 0.f);

        return std::pair<Scalar, Scalar>{max_bevel_top, max_bevel_bottom};
    }
};

///
/// Generate a rounded cone mesh.
///
/// Creates a cone mesh with optionally rounded edges at the top and bottom. The cone is centered at
/// 1/2 of its height along its center axis. The cone can have different radii at the top and
/// bottom, making it suitable for generating cones, cylinders, truncated cones and even sphere.
///
/// When bevel_radius_top or bevel_radius_bottom > 0, the respective edges are rounded using
/// toroidal patches. The smoothness of the rounding is controlled by the corresponding
/// bevel_segments parameters.
///
/// Partial cones can be generated by specifying start_sweep_angle and end_sweep_angle different
/// from 0 and 2π respectively.
///
/// @tparam Scalar  The scalar type for vertex coordinates
/// @tparam Index   The index type for face indices
///
/// @param setting  Configuration parameters for the rounded cone generation
///
/// @return A surface mesh representing the rounded cone
///
/// @note The function automatically calls setting.project_to_valid_range() to ensure
///       all parameters are within valid bounds before generation.
///
/// @note When radius_top equals radius_bottom, the result is a cylinder.
///       When radius_top is 0, the result is a traditional cone.
///
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_rounded_cone(RoundedConeOptions setting);

/// @}

} // namespace lagrange::primitive

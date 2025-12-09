/*
 * Copyright 2021 Adobe. All rights reserved.
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
    #include <lagrange/primitive/legacy/generate_swept_surface.h>
#endif

#include <lagrange/SurfaceMesh.h>
#include <lagrange/primitive/PrimitiveOptions.h>
#include <lagrange/primitive/SweepOptions.h>
#include <lagrange/utils/span.h>

#include <array>
#include <string_view>

namespace lagrange::primitive {

/// @addtogroup module-primitive
/// @{

struct SweptSurfaceOptions : public PrimitiveOptions
{
    /// The name of the output vertex attribute storing the latitude values.
    /// If empty, no latitude attribute will be created.
    std::string_view latitude_attribute_name = "@latitude";

    /// The name of the output vertex attribute storing the longitude values.
    /// If empty, no longitude attribute will be created.
    std::string_view longitude_attribute_name = "@longitude";

    /// Whether to parameterize the profile length as the U coordinate in the UV mapping.
    /// If `false`, the V coordinate will be used for the profile length.
    bool use_u_as_profile_length = true;

    /// The maximum allowed angle (in radians) between consecutive profile segments for it to be
    /// considered as smooth. UV and normal will be discontinuous across non-smooth segments.
    float profile_angle_threshold = static_cast<float>(lagrange::internal::pi / 4);

    /// Split the profile curve into shorter segments for UV generation such that no segment exceeds
    /// this length. If the value is non-positive, no splitting will be performed.
    float max_profile_length = 0.0f;
};

///
/// Generate a swept surface from a profile curve and a sequence of transforms.
///
/// @param[in]  profile       A simply connected 2D curve (flattened N by 2 array in row major)
///                           serving as sweep profile.
/// @param[in]  sweep_setting A set of sweep settings defining the sweep path.
/// @param[in]  options       Options for generating the swept surface.
///
/// @return     A SurfaceMesh representing the swept surface.
///
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_swept_surface(
    span<const Scalar> profile,
    const SweepOptions<Scalar>& sweep_setting,
    const SweptSurfaceOptions& options = {});

/// @}

} // namespace lagrange::primitive

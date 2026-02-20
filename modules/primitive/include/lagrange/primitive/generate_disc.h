/*
 * Copyright 2025 Adobe. All rights reserved.
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
#include <lagrange/primitive/PrimitiveOptions.h>

#include <array>

namespace lagrange::primitive {

/// @addtogroup module-primitive
/// @{

struct DiscOptions : public PrimitiveOptions
{
    using Scalar = PrimitiveOptions::Scalar;

    /// Radius of the disc.
    Scalar radius = 1.0;

    /// Start angle of the disc in radians.
    Scalar start_angle = 0.0;

    /// End angle of the disc in radians.
    Scalar end_angle = static_cast<Scalar>(2 * lagrange::internal::pi);

    /// Number of radial sections (spokes) in the disc.
    size_t radial_sections = 32;

    /// Number of concentric rings in the disc.
    size_t num_rings = 1;

    /// Unit normal vector for the disc.
    std::array<Scalar, 3> normal = {0, 0, 1};

    void project_to_valid_range()
    {
        radius = std::max(radius, Scalar(0));
        radial_sections = std::max(radial_sections, size_t(3));
        num_rings = std::max(num_rings, size_t(1));
    }
};

///
/// Generates a disc mesh with the specified settings.
///
/// The disc will be centered at `setting.center`, with its normal pointing along `setting.normal`.
///
/// @param setting The settings for the disc mesh.
///
/// @return A `SurfaceMesh` object representing the generated disc.
///
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_disc(DiscOptions setting);

/// @}

} // namespace lagrange::primitive

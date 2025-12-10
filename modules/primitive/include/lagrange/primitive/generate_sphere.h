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
    #include <lagrange/primitive/legacy/generate_sphere.h>
#endif

#include <lagrange/SurfaceMesh.h>
#include <lagrange/internal/constants.h>
#include <lagrange/primitive/PrimitiveOptions.h>

namespace lagrange::primitive {

/// @addtogroup module-primitive
/// @{

///
/// Options for generating a sphere mesh.
///
struct SphereOptions : public PrimitiveOptions
{
    using Scalar = PrimitiveOptions::Scalar;
    using Index = size_t;

    /// Sphere radius
    Scalar radius = 1;

    /// Sphere starting angle in radians.
    Scalar start_sweep_angle = 0;

    /// Sphere ending angle in radians.
    Scalar end_sweep_angle = static_cast<Scalar>(2 * lagrange::internal::pi);

    /// Number of sections along the longitude (vertical) direction.
    Index num_longitude_sections = 32;

    /// Number of sections along the latitude (horizontal) direction.
    Index num_latitude_sections = 32;

    ///
    /// Project setting into valid range.
    ///
    void project_to_valid_range()
    {
        radius = std::max(radius, Scalar(0.0));
        num_longitude_sections = std::max(num_longitude_sections, static_cast<Index>(3));
        num_latitude_sections = std::max(num_latitude_sections, static_cast<Index>(3));
    }
};

///
/// Generate a sphere mesh based on the specified settings.
///
/// @tparam Scalar The scalar type of the mesh.
/// @tparam Index The index type of the mesh.
///
/// @param setting The settings for the sphere mesh generation.
///
/// @return A SurfaceMesh object representing the generated sphere.
///
/// @note The function automatically calls setting.project_to_valid_range() to ensure
///       all parameters are within valid bounds before generation.
///
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_sphere(SphereOptions setting);

/// @}

} // namespace lagrange::primitive

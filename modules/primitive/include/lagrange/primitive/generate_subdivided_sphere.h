/*
 * Copyright 2020 Adobe. All rights reserved.
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
    #include <lagrange/primitive/legacy/generate_subdivided_sphere.h>
#endif

#include <lagrange/primitive/PrimitiveOptions.h>

namespace lagrange::primitive {

/// @addtogroup module-primitive
/// @{

struct SubdividedSphereOptions : public PrimitiveOptions
{
    using Scalar = PrimitiveOptions::Scalar;
    using Index = size_t;

    /// Radius of the subdivided sphere.
    Scalar radius = 1;

    /// Subdivision level for the sphere mesh.
    Index subdiv_level = 0;

    ///
    /// Clamps all parameters to valid ranges.
    ///
    /// This method ensures that:
    /// - The radius is non-negative
    /// - The subdivision level is non-negative
    ///
    /// Invalid parameters are automatically corrected to prevent geometric inconsistencies.
    ///
    void project_to_valid_range()
    {
        radius = std::max(radius, Scalar(0));
        subdiv_level = std::max(subdiv_level, Index(0));
    }
};

///
/// Generate a subdivided sphere mesh from a base shape.
///
/// The function takes a base shape (e.g., an icosahedron or octahedron) and performs
/// subdivision followed by normalization to create a smoother sphere mesh.
///
/// @tparam Scalar  The scalar type for vertex coordinates.
/// @tparam Index   The index type for face indices.
///
/// @param base_shape  The initial coarse mesh to be subdivided.
/// @param setting     Configuration parameters for the subdivided sphere generation.
///
/// @return A surface mesh representing the subdivided sphere.
///
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_subdivided_sphere(
    const SurfaceMesh<Scalar, Index>& base_shape,
    SubdividedSphereOptions setting);

/// @}

} // namespace lagrange::primitive

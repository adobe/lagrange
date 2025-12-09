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
    #include <lagrange/primitive/legacy/generate_octahedron.h>
#endif

#include <lagrange/primitive/PrimitiveOptions.h>

namespace lagrange::primitive {

/// @addtogroup module-primitive
/// @{

///
/// Options for generating an octahedron mesh.
///
struct OctahedronOptions : public PrimitiveOptions
{
    using Scalar = PrimitiveOptions::Scalar;

    /// Radius of the circumscribed sphere around the octahedron.
    Scalar radius = 1;

    ///
    /// Clamps all parameters to valid ranges.
    ///
    /// This method ensures that:
    /// - The radius is non-negative
    ///
    /// Invalid parameters are automatically corrected to prevent geometric inconsistencies.
    ///
    void project_to_valid_range() { radius = std::max(radius, Scalar(0)); }
};

///
/// Generate an octahedron mesh.
///
/// Creates an octahedron mesh with the specified radius. An octahedron is a regular polyhedron
/// with 8 triangular faces, 12 edges, and 6 vertices. It is one of the five Platonic solids
/// and exhibits octahedral symmetry (Oh).
///
/// @tparam Scalar  The scalar type for vertex coordinates
/// @tparam Index   The index type for face indices
///
/// @param settings  Configuration parameters for the octahedron generation
///
/// @return A surface mesh representing the octahedron
///
/// @note The function automatically calls settings.project_to_valid_range() to ensure
///       all parameters are within valid bounds before generation.
///
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_octahedron(OctahedronOptions settings);

/// @}

} // namespace lagrange::primitive

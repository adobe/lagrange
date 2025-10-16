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
#include <lagrange/image/View3D.h>

#include <string_view>

namespace lagrange::texproc {

/// @addtogroup module-texproc
/// @{

///
/// Options for texture stitching.
///
struct StitchingOptions
{
    /// If true, interior texels are fixed degrees of freedom.
    bool exterior_only = false;

    /// The number of quadrature samples to use for integration (in {1, 3, 6, 12, 24, 32}).
    unsigned int quadrature_samples = 6;

    /// Jitter amount per texel (0 to deactivate).
    double jitter_epsilon = 1e-4;

    /// Initially the boundary texels to random values (for debugging purposes).
    bool __randomize = false;
};

///
/// Stitch the seams of a texture associated with a mesh.
///
/// @param[in]     mesh       Input mesh with UV attributes.
/// @param[in,out] texture    Texture to stitch.
/// @param[in]     options    Stitching options.
///
/// @tparam        Scalar     Mesh scalar type.
/// @tparam        Index      Mesh index type.
/// @tparam        ValueType  Image value type.
///
template <typename Scalar, typename Index, typename ValueType>
void texture_stitching(
    const SurfaceMesh<Scalar, Index>& mesh,
    image::experimental::View3D<ValueType> texture,
    const StitchingOptions& options = {});

/// @}

} // namespace lagrange::texproc

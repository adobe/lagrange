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
/// Options for texture filtering (smoothing or sharpening).
///
struct FilteringOptions
{
    /// The weight for fitting the values of the signal.
    double value_weight = 1e3;

    /// The weight for fitting the modulated gradients of the signal.
    ///
    /// @note       There is no reason to have modify both the value-weight and the gradient-weight.
    ///             Results obtained by scaling the value-weight by a factor should be equivalent to
    ///             scaling the gradient-weight by the reciprocal of the factor.
    double gradient_weight = 1.;

    /// The gradient modulation weight. Use a value of 0 for smoothing, and use a value
    /// between [2, 10] for sharpening.
    double gradient_scale = 1.;

    /// The number of quadrature samples to use for integration (in {1, 3, 6, 12, 24, 32}).
    unsigned int quadrature_samples = 6;

    /// Jitter amount per texel (0 to deactivate).
    double jitter_epsilon = 1e-4;
};

///
/// Smooth or sharpen a texture image associated with a mesh.
///
/// @param[in]     mesh       Input mesh with UV attributes.
/// @param[in,out] texture    Texture image to filter.
/// @param[in]     options    Filtering options.
///
/// @tparam        Scalar     Mesh scalar type.
/// @tparam        Index      Mesh index type.
/// @tparam        ValueType  Texture value type.
///
template <typename Scalar, typename Index, typename ValueType>
void texture_filtering(
    const SurfaceMesh<Scalar, Index>& mesh,
    image::experimental::View3D<ValueType> texture,
    const FilteringOptions& options = {});

/// @}

} // namespace lagrange::texproc

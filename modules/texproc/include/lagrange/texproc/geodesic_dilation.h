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

struct DilationOptions
{
    /// The radius by which the texture should be dilated into the gutter.
    unsigned int dilation_radius = 10;

    /// If true, write a dilated position map instead to the output texture image.
    bool output_position_map = false;
};

///
/// Extend pixels of a texture beyond the defined UV mesh by walking along the 3D surface.
///
/// @param[in]     mesh       Input mesh with UV attributes.
/// @param[in,out] texture    Texture to extend beyond UV mesh boundaries, or where to write the
///                           output position map. When writing a position map, the texture must
///                           have 3 channels.
/// @param[in]     options    Dilation options.
///
/// @tparam        Scalar     Mesh scalar type.
/// @tparam        Index      Mesh index type.
/// @tparam        ValueType  Image value type.
///
template <typename Scalar, typename Index, typename ValueType>
void geodesic_dilation(
    const SurfaceMesh<Scalar, Index>& mesh,
    image::experimental::View3D<ValueType> texture,
    const DilationOptions& options = {});

/// @}

} // namespace lagrange::texproc

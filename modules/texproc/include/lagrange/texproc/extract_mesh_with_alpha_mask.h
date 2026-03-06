/*
 * Copyright 2026 Adobe. All rights reserved.
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

#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/image/View3D.h>

namespace lagrange::texproc {

///
/// Extract mesh with alpha mask options.
///
struct ExtractMeshWithAlphaMaskOptions
{
    /// Indexed UV attribute id.
    /// Must be a valid attribute of the input mesh.
    AttributeId texcoord_id = invalid_attribute_id();

    /// Opaque mask theshold.
    float alpha_threshold = 0.5f;
};

///
/// Convert a unwrapped triangle mesh with non-opaque texture to tesselated mesh.
///
/// @param[in]  mesh            Input mesh.
/// @param[in]  image           RGBA non-opaque texture.
/// @param[in]  options         Extraction options.
///
/// @tparam     Scalar      Mesh scalar type.
/// @tparam     Index       Mesh index type.
///
/// @return     Tesselated mesh.
///
template <typename Scalar, typename Index>
auto extract_mesh_with_alpha_mask(
    const SurfaceMesh<Scalar, Index>& mesh,
    const image::experimental::View3D<const float> image,
    const ExtractMeshWithAlphaMaskOptions& options) -> SurfaceMesh<Scalar, Index>;

} // namespace lagrange::texproc

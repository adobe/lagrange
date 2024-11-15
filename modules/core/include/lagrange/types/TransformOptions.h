/*
 * Copyright 2024 Adobe. All rights reserved.
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

namespace lagrange {

///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities.
///
/// @{
///

///
/// Options available when applying affine transforms to a mesh.
///
struct TransformOptions
{
    /// If enabled, normals are normalized after transformation.
    bool normalize_normals = true;

    /// If enabled, tangents and bitangents are normalized after transformation.
    bool normalize_tangents_bitangents = true;

    /// If enabled, when applying a transform with negative determinant:
    /// 1. Normals, tangents and bitangents are flipped.
    /// 2. Facet orientations are reversed.
    bool reorient = false;
};

///
/// @}
///

} // namespace lagrange

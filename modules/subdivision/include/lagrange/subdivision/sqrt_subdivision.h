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

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/subdivision/legacy/sqrt_subdivision.h>
#endif

#include <lagrange/SurfaceMesh.h>

namespace lagrange::subdivision {

///
/// Performs one step of sqrt(3)-subdivision. Implementation based on:
///
/// <BLOCKQUOTE> Kobbelt, Leif. "√3-subdivision." Proceedings of the 27th annual conference on
/// Computer graphics and interactive techniques. 2000. https://doi.org/10.1145/344779.344835
/// </BLOCKQUOTE>
///
/// @note       This function currently does not remap any mesh attribute.
///
/// @param[in]  mesh    Input mesh to subdivide. Only triangle meshes are supported.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
/// @return     Subdivided mesh.
///
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> sqrt_subdivision(const SurfaceMesh<Scalar, Index>& mesh);

} // namespace lagrange::subdivision

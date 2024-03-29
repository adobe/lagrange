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
    #include <lagrange/subdivision/legacy/midpoint_subdivision.h>
#endif

#include <lagrange/SurfaceMesh.h>

namespace lagrange::subdivision {

/// @addtogroup module-subdivision
/// @{

///
/// Performs one step of midpoint subdivision for triangle meshes.
///
/// @note          This function currently does not remap any mesh attribute.
///
/// @param[in,out] mesh    Input mesh to subdivide.
///
/// @tparam        Scalar  Mesh scalar type.
/// @tparam        Index   Mesh index type.
///
/// @return        Subdivided mesh.
///
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> midpoint_subdivision(const SurfaceMesh<Scalar, Index>& mesh);

/// @}

} // namespace lagrange::subdivision

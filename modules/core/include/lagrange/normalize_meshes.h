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
    #include <lagrange/legacy/normalize_meshes.h>
#endif

#include <lagrange/SurfaceMesh.h>
#include <lagrange/utils/span.h>

#include <vector>

namespace lagrange {

///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities.
///
/// @{

///
/// Normalize a mesh to fit in a unit box centered at the origin.
///
/// @param[in]  mesh    Input mesh.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
template <typename Scalar, typename Index>
void normalize_mesh(SurfaceMesh<Scalar, Index>& mesh);

///
/// Normalize a list of meshes to fit in a unit box centered at the origin.
///
/// @param[in]  meshes  List of pointers to the meshes to modify.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
template <typename Scalar, typename Index>
void normalize_meshes(span<SurfaceMesh<Scalar, Index>*> meshes);

/// @}

} // namespace lagrange

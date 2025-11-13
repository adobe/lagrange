/*
 * Copyright 2018 Adobe. All rights reserved.
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
    #include <lagrange/mesh_cleanup/legacy/resolve_vertex_nonmanifoldness.h>
#endif

#include <lagrange/SurfaceMesh.h>

namespace lagrange {

///
/// Resolve nonmanifold vertices by pulling disconnected 1-ring
/// neighborhood apart.
///
/// @tparam Scalar The scalar type.
/// @tparam Index The index type.
///
/// @param[in,out] mesh  The input mesh, which will be updated in place.
///
/// @warning This function assumes the input mesh contains **no** nonmanifold
/// edges or inconsistently oriented triangles.
///
template <typename Scalar, typename Index>
void resolve_vertex_nonmanifoldness(SurfaceMesh<Scalar, Index>& mesh);

} // namespace lagrange

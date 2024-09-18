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
    #include <lagrange/mesh_cleanup/legacy/resolve_nonmanifoldness.h>
#endif

namespace lagrange {

///
/// Resolve both non-manifold vertices and non-manifold edges in the input mesh.
///
/// @tparam Scalar  Mesh scalar type
/// @tparam Index   Mesh index type
///
/// @param mesh     Input mesh, which will be updated in place
///
template <typename Scalar, typename Index>
void resolve_nonmanifoldness(SurfaceMesh<Scalar, Index>& mesh);

} // namespace lagrange

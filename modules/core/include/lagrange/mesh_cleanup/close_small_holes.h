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
    #include <lagrange/mesh_cleanup/legacy/close_small_holes.h>
#endif

#include <lagrange/SurfaceMesh.h>

namespace lagrange {

///
/// Option struct for closing small holes.
///
struct CloseSmallHolesOptions
{
    /// Maximum number of vertices on a hole to be closed.
    size_t max_hole_size = 16;

    /// Whether to triangulate holes. If false, holes will be filled by polygons.
    bool triangulate_holes = true;
};

///
/// Close small topological holes.
///
/// @param[in,out] mesh    Input mesh whose holes to close in place.
/// @param[in]     options Optional arguments to control hole closing.
///
template <typename Scalar, typename Index>
void close_small_holes(SurfaceMesh<Scalar, Index>& mesh, CloseSmallHolesOptions options = {});

} // namespace lagrange

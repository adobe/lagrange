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

#include <lagrange/SurfaceMesh.h>

namespace lagrange::bvh {

///
/// Removes interior shells from a (manifold, non-intersecting) mesh.
///
/// This function assumes that the connected components are manifold without open boundaries (e.g.
/// such as isosurfaces extracted from a SDF). It removes all shells that are fully enclosed by
/// other shells, keeping only the outermost shells.
///
/// @param      mesh  Input mesh to process.
///
/// @return     A new mesh containing only the exterior shells.
///
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> remove_interior_shells(const SurfaceMesh<Scalar, Index>& mesh);

} // namespace lagrange::bvh

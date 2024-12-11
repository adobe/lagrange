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

#include <lagrange/SurfaceMesh.h>

namespace lagrange {

///
/// Removes degenerate facets from a mesh.
///
/// @note This method currently only works with triangular meshes.
///       Only exactly degenerate facets are detected.
///
/// @param[in,out] mesh Input mesh which will be modified in place.
///
/// @note The current method assumes input mesh is triangular.
///       Use `triangulate_polygonal_facets` if the input mesh is not triangular.
///       Non-degenerate facets adjacent to degenerate facets may be re-triangulated as a result of
///       the removal.
///
template <typename Scalar, typename Index>
void remove_degenerate_facets(SurfaceMesh<Scalar, Index>& mesh);

} // namespace lagrange

/*
 * Copyright 2023 Adobe. All rights reserved.
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

#include <vector>

namespace lagrange {

///
/// Detects degenerate facets in a mesh.
///
/// @param mesh Input mesh.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
/// @return A vector of indices of degenerate facets.
///
/// @note Only exactly degenerate facets are detected.
///       For polygonal facets, degeneracy is defined as all vertices are colinear.
///
/// @see @ref remove_degenerate_facets
///
template <typename Scalar, typename Index>
std::vector<Index> detect_degenerate_facets(const SurfaceMesh<Scalar, Index>& mesh);

} // namespace lagrange

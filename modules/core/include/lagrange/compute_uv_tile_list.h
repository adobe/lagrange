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
#include <utility>
#include <vector>

namespace lagrange {

///
/// Extract the list of all UV tiles that a mesh's parametrization spans.
/// UV tiles are usually understood to be a regular unit grid in UV space.
/// This process thus reads UV for all vertices of the input mesh, and adds an entry
/// to the output for each new integer pair that it finds.
///
/// @param      mesh        Mesh to be analyzed
///
/// @return     A list of integer coordinates with one entry for each UV tile of mesh.
///
template <typename Scalar, typename Index>
std::vector<std::pair<int32_t, int32_t>> compute_uv_tile_list(
    const SurfaceMesh<Scalar, Index>& mesh);

} // namespace lagrange

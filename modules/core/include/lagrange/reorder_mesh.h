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

namespace lagrange {

///
/// Mesh reordering method to apply before decimation.
///
enum class ReorderingMethod {
    Lexicographic, ///< Sort vertices/facets lexicographically
    Morton, ///< Spatial sort vertices/facets using Morton encoding
    Hilbert, ///< Spatial sort vertices/facets using Hilbert curve
    None, ///< Do not reorder mesh vertices/facets
};

///
/// Mesh reordering to improve cache locality. The reordering is done in place.
///
/// @param[in,out] mesh    Mesh to reorder in place. For now we only support triangle meshes.
/// @param[in]     method  Reordering method.
///
/// @tparam        Scalar  Mesh scalar type.
/// @tparam        Index   Mesh index type.
///
template <typename Scalar, typename Index>
void reorder_mesh(SurfaceMesh<Scalar, Index>& mesh, ReorderingMethod method);

} // namespace lagrange

/*
 * Copyright 2022 Adobe. All rights reserved.
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
/// @defgroup   group-surfacemesh-views Polygon triangulation
/// @ingroup    group-surfacemesh
///

///
/// Triangulate polygonal facets of a mesh using a prescribed set of rules.
///
/// @param[in, out] mesh     Polygonal mesh to triangulate in place.
///
/// @tparam         Scalar   Mesh scalar type.
/// @tparam         Index    Mesh index type.
///
template <typename Scalar, typename Index>
void triangulate_polygonal_facets(SurfaceMesh<Scalar, Index>& mesh);

} // namespace lagrange

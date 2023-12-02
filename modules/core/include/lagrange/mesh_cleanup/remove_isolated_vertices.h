/*
 * Copyright 2017 Adobe. All rights reserved.
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

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/mesh_cleanup/legacy/remove_isolated_vertices.h>
#endif

namespace lagrange {

///
/// Removes isolated vertices of a mesh. Facets incident to any removed vertex will be deleted.
///
/// @param      mesh    Mesh to modify in place.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
template <typename Scalar, typename Index>
void remove_isolated_vertices(SurfaceMesh<Scalar, Index>& mesh);

} // namespace lagrange

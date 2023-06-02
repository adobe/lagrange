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
    #include <lagrange/legacy/extract_boundary_loops.h>
#endif

#include <lagrange/SurfaceMesh.h>


namespace lagrange {

///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities.
///
/// @{

///
/// Extract boundary loops from a surface mesh.
///
/// @tparam Scalar  The scalar type of the mesh.
/// @tparam Index   The index type of the mesh.
///
/// @param mesh     The input surface mesh.
///
/// @return         A vector of boundary loops, each represented by a vector of vertex indices.
///
template <typename Scalar, typename Index>
std::vector<std::vector<Index>> extract_boundary_loops(const SurfaceMesh<Scalar, Index>& mesh);

/// @}

} // namespace lagrange

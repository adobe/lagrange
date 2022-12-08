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

#include <tuple>
#include <vector>

#include <lagrange/SurfaceMesh.h>
#include <lagrange/utils/AdjacencyList.h>

namespace lagrange {

///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities.
///
/// @{

/**
 * Compute vertex-vertex adjacency information.
 *
 * @tparam Scalar  Mesh scalar type.
 * @tparam Index   Mesh index type.
 *
 * @param mesh     The input mesh.
 *
 * @return         The vertex-vertex adjacency data and adjacency indices.
 */
template <typename Scalar, typename Index>
AdjacencyList<Index> compute_vertex_vertex_adjacency(SurfaceMesh<Scalar, Index>& mesh);

/// @}

} // namespace lagrange

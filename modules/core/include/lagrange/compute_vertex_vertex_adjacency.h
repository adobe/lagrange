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
#include <lagrange/types/DualConnectivityType.h>
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
 * Two vertices are considered adjacent based on the connectivity type:
 * - DualConnectivityType::Edge: Two vertices are adjacent if they are connected by a mesh edge
 *   (i.e., they are consecutive vertices in some facet). This is the default.
 * - DualConnectivityType::Facet: Two vertices are adjacent if they belong to the same facet
 *   (includes diagonal connections within a polygon).
 *
 * The resulting adjacency list is deduplicated: each neighbor appears at most once per vertex.
 *
 * @tparam Scalar            Mesh scalar type.
 * @tparam Index             Mesh index type.
 *
 * @param mesh               The input mesh.
 * @param connectivity_type  Adjacency condition (default: Edge).
 *
 * @return                   The vertex-vertex adjacency list.
 */
template <typename Scalar, typename Index>
AdjacencyList<Index> compute_vertex_vertex_adjacency(
    SurfaceMesh<Scalar, Index>& mesh,
    DualConnectivityType connectivity_type = DualConnectivityType::Edge);

/// @}

} // namespace lagrange

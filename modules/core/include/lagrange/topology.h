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
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities.
///
/// @{

///
/// Compute Euler characteristic of a mesh.
///
/// @tparam Scalar  The scalar type of the mesh.
/// @tparam Index   The index type of the mesh.
///
/// @param mesh     The input mesh.
///
/// @return         The Euler characteristic of the mesh.
///
template <typename Scalar, typename Index>
int compute_euler(const SurfaceMesh<Scalar, Index>& mesh);

///
/// Check if a mesh is vertex-manifold.
///
/// A mesh is vertex-manifold if and only if the one-ring neighborhood of each vertex is of disc
/// topology. I.e. The boundary of the 1-ring neighborhood is a simple loop for interior
/// vertices, and a simple chain for boundary vertices.
///
/// @tparam Scalar  The scalar type of the mesh.
/// @tparam Index   The index type of the mesh.
///
/// @param mesh     The input mesh.
///
/// @return         True if the mesh is vertex-manifold.
///
template <typename Scalar, typename Index>
bool is_vertex_manifold(const SurfaceMesh<Scalar, Index>& mesh);

///
/// Check if a mesh is edge-manifold.
///
/// A mesh is edge-manifold if and only if every interior edge is incident to exactly two facets,
/// and every boundary edge is incident to exactly one facet.
///
/// @tparam Scalar  The scalar type of the mesh.
/// @tparam Index   The index type of the mesh.
///
/// @param mesh     The input mesh.
///
/// @return         True if the mesh is edge-manifold.
///
template <typename Scalar, typename Index>
bool is_edge_manifold(const SurfaceMesh<Scalar, Index>& mesh);

///
/// Check if a mesh is both vertex-manifold and edge-manifold.
///
/// @tparam Scalar  The scalar type of the mesh.
/// @tparam Index   The index type of the mesh.
///
/// @param mesh     The input mesh.
///
/// @return         True if the mesh is both vertex-manifold and edge-manifold.
///
template <typename Scalar, typename Index>
bool is_manifold(const SurfaceMesh<Scalar, Index>& mesh)
{
    return is_edge_manifold(mesh) && is_vertex_manifold(mesh);
}

/// @}
} // namespace lagrange

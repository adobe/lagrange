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
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities.
///
/// @{

///
/// Check if a mesh is oriented.
///
/// A mesh is oriented if interior edges have the same number of half-edges for each of
/// the edge direction.
///
/// @param      mesh    The input mesh.
///
/// @tparam     Scalar  The scalar type of the mesh.
/// @tparam     Index   The index type of the mesh.
///
/// @return     True if the mesh is oriented.
///
template <typename Scalar, typename Index>
bool is_oriented(const SurfaceMesh<Scalar, Index>& mesh);

///
/// Option struct for computing if edges are oriented.
///
struct OrientationOptions
{
    /// Per-edge attribute indicating whether an edge is oriented.
    std::string_view output_attribute_name = "@edge_is_oriented";
};

///
/// Compute a mesh attribute indicating whether an edge is oriented.
///
/// @param[in,out] mesh     Input mesh.
/// @param[in]     options  Output attribute options.
///
/// @tparam        Scalar   Mesh scalar type.
/// @tparam        Index    Mesh index type.
///
/// @return     Id of the newly added per-edge attribute, of type uint8_t.
///
template <typename Scalar, typename Index>
AttributeId compute_edge_is_oriented(
    SurfaceMesh<Scalar, Index>& mesh,
    const OrientationOptions& options = {});

/// @}

} // namespace lagrange

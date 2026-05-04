/*
 * Copyright 2026 Adobe. All rights reserved.
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

#include <Eigen/Geometry>

namespace lagrange {

///
/// @addtogroup group-surfacemesh-utils
/// @{
///

///
/// Compute the axis-aligned bounding box of a mesh.
///
/// If the mesh has no vertices, the returned bounding box is empty (default-constructed
/// `Eigen::AlignedBox`, where `isEmpty()` returns true).
///
/// @param[in]  mesh       Input mesh. Its dimension must match the @p Dimension template parameter.
///
/// @tparam     Dimension  Spatial dimension of the bounding box. Must be 2 or 3.
/// @tparam     Scalar     Mesh scalar type.
/// @tparam     Index      Mesh index type.
///
/// @return     The axis-aligned bounding box of the mesh vertices.
///
template <size_t Dimension, typename Scalar, typename Index>
Eigen::AlignedBox<Scalar, static_cast<int>(Dimension)> mesh_bbox(
    const SurfaceMesh<Scalar, Index>& mesh);

/// @}

} // namespace lagrange

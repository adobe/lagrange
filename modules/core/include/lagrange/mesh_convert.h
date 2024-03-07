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

#include <lagrange/Mesh.h>
#include <lagrange/SurfaceMesh.h>

namespace lagrange {
///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various attribute processing utilities
///
/// @{

///
/// Convert a legacy mesh object to a surface mesh object.
///
/// @param[in]  mesh      Mesh object to convert.
///
/// @tparam     Scalar    Output mesh scalar type. Must be either float or double.
/// @tparam     Index     Output mesh index type. Must be either uint32_t or uint64_t.
/// @tparam     MeshType  Input mesh type.
///
/// @return     New mesh object.
///
template <typename Scalar, typename Index, typename MeshType>
SurfaceMesh<Scalar, Index> to_surface_mesh_copy(const MeshType& mesh);

///
/// Wrap a legacy mesh object as a surface mesh object. The mesh scalar & index types must match.
///
/// @param[in]  mesh      Mesh object to convert. The mesh object must be a lvalue reference (no
///                       temporary).
///
/// @tparam     Scalar    Output mesh scalar type. Must be either float or double.
/// @tparam     Index     Output mesh index type. Must be either uint32_t or uint64_t.
/// @tparam     MeshType  Input mesh type.
///
/// @return     New mesh object.
///
template <typename Scalar, typename Index, typename MeshType>
SurfaceMesh<Scalar, Index> to_surface_mesh_wrap(MeshType&& mesh);

///
/// Convert a surface mesh object to a legacy mesh object. The mesh must be a regular mesh object.
///
/// @param[in]  mesh      Mesh object to convert.
///
/// @tparam     MeshType  Output mesh type.
/// @tparam     Scalar    Input mesh scalar type. Must be either float or double.
/// @tparam     Index     Input mesh index type. Must be either uint32_t or uint64_t.
///
/// @return     New mesh object.
///
template <typename MeshType, typename Scalar, typename Index>
std::unique_ptr<MeshType> to_legacy_mesh(const SurfaceMesh<Scalar, Index>& mesh);

/// @}
} // namespace lagrange

#include <lagrange/mesh_convert.impl.h>

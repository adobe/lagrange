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
#include <lagrange/internal/SurfaceMeshInfo.h>

namespace lagrange::internal {

///
/// Extract a SurfaceMeshInfo from a SurfaceMesh.
///
/// The returned info contains non-owning spans pointing into the mesh's attribute buffers. The mesh
/// must outlive the returned SurfaceMeshInfo.
///
/// @param[in]  mesh    The source mesh.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
/// @return     A SurfaceMeshInfo with spans into the mesh's data.
///
template <typename Scalar, typename Index>
SurfaceMeshInfo from_surface_mesh(const SurfaceMesh<Scalar, Index>& mesh);

///
/// Reconstruct a SurfaceMesh from a SurfaceMeshInfo.
///
/// This directly populates the mesh's internal data structures, avoiding expensive topology
/// reconstruction (e.g. initialize_edges). All attributes, including reserved ones, are restored
/// from the stored byte data.
///
/// @param[in]  info    The mesh info to restore from.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
/// @return     The reconstructed mesh.
///
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> to_surface_mesh(const SurfaceMeshInfo& info);

} // namespace lagrange::internal

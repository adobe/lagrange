/*
 * Copyright 2021 Adobe. All rights reserved.
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
#include <lagrange/volume/types.h>

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/volume/legacy/volume_to_mesh.h>
#endif

namespace lagrange::volume {

/// Volume to mesh isosurfacing options.
struct VolumeToMeshOptions
{
    // Value of the isosurface.
    double isovalue = 0.0;

    /// Surface adaptivity threshold [0 to 1]. 0 keeps the original quad mesh, while 1 simplifies the most.
    double adaptivity = 0.0;

    /// Toggle relaxing disoriented triangles during adaptive meshing.
    bool relax_disoriented_triangles = true;

    /// If non-empty, compute vertex normals from the volume and store them in the appropriately named attribute
    std::string_view normal_attribute_name = "";
};

///
/// Mesh the isosurface of a OpenVDB sparse voxel grid.
///
/// @param[in]  grid        Input grid.
/// @param[in]  options     Isosurfacing options.
///
/// @tparam     MeshType    Output mesh type.
/// @tparam     GridScalar  Grid scalar type. Can only be float or double.
///
/// @return     Meshed isosurface.
///
template <typename MeshType, typename GridScalar>
auto volume_to_mesh(const Grid<GridScalar>& grid, const VolumeToMeshOptions& options = {})
    -> SurfaceMesh<typename MeshType::Scalar, typename MeshType::Index>;

} // namespace lagrange::volume

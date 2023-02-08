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
    #include <lagrange/volume/legacy/mesh_to_volume.h>
#endif

namespace lagrange::volume {

///
/// Mesh to volume conversion options.
///
struct MeshToVolumeOptions
{
    ///
    /// Available methods to compute the sign of the distance field (i.e. which voxels are inside or
    /// outside of the input volume).
    ///
    enum class Sign {
        FloodFill, ///< Default voxel flood-fill method used by OpenVDB.
        WindingNumber, ///< Fast winding number approach based on [Barill et al. 2018].
    };

    /// Grid voxel size. If the target voxel size is too small, an exception will will be raised. A
    /// negative value is interpreted as being relative to the mesh bbox diagonal.
    double voxel_size = -0.01;

    /// Method used to compute the sign of the distance field that determines interior voxels.
    Sign signing_method = Sign::FloodFill;
};

///
/// Converts a triangle mesh to a OpenVDB sparse voxel grid.
///
/// @param[in]  mesh        Input mesh. Must be a triangle mesh, a quad-mesh, or a quad-dominant
///                         mesh.
/// @param[in]  options     Conversion options.
///
/// @tparam     GridScalar  Output OpenVDB Grid scalar type. Only float or double are supported.
/// @tparam     Scalar      Mesh scalar type.
/// @tparam     Index       Mesh index type.
///
/// @return     OpenVDB grid.
///
template <typename GridScalar = float, typename Scalar, typename Index>
auto mesh_to_volume(const SurfaceMesh<Scalar, Index>& mesh, const MeshToVolumeOptions& options = {})
    -> typename Grid<GridScalar>::Ptr;

} // namespace lagrange::volume

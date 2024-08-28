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

#include <string_view>

namespace lagrange::poisson {

///
/// Option struct for Poisson surface reconstruction.
///
struct ReconstructionOptions
{
    /// Input normal attribute name. If empty, uses the first attribute with usage `Normal`.
    std::string_view input_normals;

    /// Maximum octree depth.
    int octree_depth = 8;

    // <Add other parameters here>
};

///
/// Reconstruct a triangle mesh from an oriented point cloud.
///
/// @param[in]  points   Input point clouds with normal attributes.
/// @param[in]  options  Reconstruction options.
///
/// @tparam     Scalar   Mesh scalar type.
/// @tparam     Index    Mesh index type.
///
/// @return     Reconstructed triangle mesh.
///
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> mesh_from_oriented_points(
    const SurfaceMesh<Scalar, Index>& points,
    const ReconstructionOptions& options = {});

/// @}

} // namespace lagrange::poisson

/*
 * Copyright 2025 Adobe. All rights reserved.
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

namespace lagrange::volume {

/// Normals from volume options.
struct NormalsFromVolumeOptions
{
    /// Where to write output vertex normal attribute.
    std::string_view normal_attribute_name = "@vertex_normal";
};

///
/// Sample vertex normals from a SDF grid.
///
/// @param[in,out] mesh        Mesh onto which to sample SDF gradient.
/// @param[in]     grid        VDB grid from which to sample the gradient.
/// @param[in]     options     Sampling options.
///
/// @tparam        Scalar      Mesh scalar type.
/// @tparam        Index       Mesh index type.
/// @tparam        GridScalar  Grid scalar type. Can only be float or double.
///
template <typename Scalar, typename Index, typename GridScalar>
void sample_vertex_normal(
    SurfaceMesh<Scalar, Index>& mesh,
    const Grid<GridScalar>& grid,
    const NormalsFromVolumeOptions& options = {});

} // namespace lagrange::volume

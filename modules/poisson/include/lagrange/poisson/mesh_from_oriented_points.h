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

    /// Maximum octree depth. (If the value is zero then log base 4 of the point count is used.)
    unsigned int octree_depth = 0;

    /// Point interpolation weight (lambda)
    float interpolation_weight = 2.f;

    /// Use normal length as confidence
    bool use_normal_length_as_confidence = false;

    /// Use dirichlet boundary conditions
    bool use_dirichlet_boundary = false;

    /// Attribute name of data to be interpolated at the vertices
    std::string_view input_output_attribute_name;
    
    /// Output density attribute
    std::string_view output_vertex_depth_attribute_name;

    /// Output logging information
    bool show_logging_output = false;
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

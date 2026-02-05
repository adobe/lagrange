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

#include <optional>

namespace lagrange::subdivision {

/// @addtogroup module-subdivision
/// @{

///
/// Attribute ids returned by the `compute_sharpness` function.
///
struct SharpnessResults
{
    /// Attribute id of the indexed normal attribute used to compute sharpness information.
    std::optional<AttributeId> normal_attr;

    /// Attribute id to use for vertex sharpness in the `subdivide_mesh` function (has `float` type).
    std::optional<AttributeId> vertex_sharpness_attr;

    /// Attribute id to use for edge sharpness in the `subdivide_mesh` function (has `float` type).
    std::optional<AttributeId> edge_sharpness_attr;
};

///
/// Input options for the `compute_sharpness` function.
///
struct SharpnessOptions
{
    /// If provided, name of the normal attribute to use as indexed normals to define sharp edges.
    /// If not provided, the function will attempt to find an existing indexed normal attribute. If
    /// no such attribute is found, autosmooth normals will be computed based on the
    /// `feature_angle_threshold` parameter.
    std::string_view normal_attribute_name;

    /// Feature angle threshold (in radians) to detect sharp edges when computing autosmooth
    /// normals. By default, if no indexed normal attribute is found, no autosmooth normals will be
    /// computed.
    std::optional<double> feature_angle_threshold;
};

///
/// Compute subdivision options to handle sharp edges and vertices based on existing mesh
/// attributes.
///
/// This function performs the following operations:
/// 1. Find an attribute to use as indexed normals to define sharp edges.
/// 2. If not found, compute indexed corner normals based on a user-defined feature angle threshold.
/// 3. Compute edge and vertex sharpness `float` attributes based on indexed normals topology.
///
/// The mesh is modified in place to add the necessary attributes. If no indexed normal attribute is
/// found, and no autosmooth feature angle threshold is provided, then no sharpness information is
/// computed.
///
/// @note           Please ensure that relevant input indexed normals are properly welded before
///                 calling this function.
///
/// @param[in, out] mesh     Input mesh to prepare for subdivision.
/// @param[in]      options  Input options for computing sharpness information.
///
/// @tparam         Scalar   Mesh scalar type.
/// @tparam         Index    Mesh index type.
///
/// @return         Normal, edge and vertex sharpness attribute ids.
///
template <typename Scalar, typename Index>
SharpnessResults compute_sharpness(
    SurfaceMesh<Scalar, Index>& mesh,
    const SharpnessOptions& options = {});

/// @}

} // namespace lagrange::subdivision

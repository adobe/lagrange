/*
 * Copyright 2020 Adobe. All rights reserved.
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
#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/legacy/normalize_meshes.h>
#endif

#include <lagrange/SurfaceMesh.h>
#include <lagrange/transform_mesh.h>
#include <lagrange/utils/span.h>

#include <Eigen/Geometry>

namespace lagrange {

///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities.
///
/// @{

///
/// Normalize a mesh to fit in a unit box centered at the origin.
///
/// @param[out]  mesh    Input mesh.
/// @param[in]   options Transform options.
///
/// @tparam     Dimension  Mesh dimension.
/// @tparam     Scalar     Mesh scalar type.
/// @tparam     Index      Mesh index type.
///
/// @return The inverse transform, can be used to undo the normalization process.
///
template <size_t Dimension = 3, typename Scalar, typename Index>
auto normalize_mesh_with_transform(
    SurfaceMesh<Scalar, Index>& mesh,
    const TransformOptions& options = {}) -> Eigen::Transform<Scalar, Dimension, Eigen::Affine>;

///
/// Normalize a mesh to fit in a unit box centered at the origin.
///
/// @param[out]  mesh    Input mesh.
/// @param[in]   options Transform options.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
template <typename Scalar, typename Index>
void normalize_mesh(SurfaceMesh<Scalar, Index>& mesh, const TransformOptions& options = {});

///
/// Normalize a list of meshes to fit in a unit box centered at the origin.
///
/// @param[out]  meshes  List of pointers to the meshes to modify.
/// @param[in]   options Transform options.
///
/// @tparam     Dimension  Mesh dimension.
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
/// @return The inverse transform, can be used to undo the normalization process.
///
template <size_t Dimension = 3, typename Scalar, typename Index>
auto normalize_meshes_with_transform(
    span<SurfaceMesh<Scalar, Index>*> meshes,
    const TransformOptions& options = {}) -> Eigen::Transform<Scalar, Dimension, Eigen::Affine>;

///
/// Normalize a list of meshes to fit in a unit box centered at the origin.
///
/// @param[out]  meshes  List of pointers to the meshes to modify.
/// @param[in]   options Transform options.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
template <typename Scalar, typename Index>
void normalize_meshes(
    span<SurfaceMesh<Scalar, Index>*> meshes,
    const TransformOptions& options = {});

/// @}

} // namespace lagrange

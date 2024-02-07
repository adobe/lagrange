/*
 * Copyright 2023 Adobe. All rights reserved.
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
#include <lagrange/types/TransformOptions.h>

#include <Eigen/Geometry>

namespace lagrange {

///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various attribute processing utilities
///
/// @{
///

///
/// Apply an affine transform @f$ M @f$ to a mesh in-place. All mesh attributes are transformed
/// based on their usage tags:
///
/// - Position: Applies @f$ P \to M * P @f$
/// - Normal: Applies @f$ P \to \det(M) M^{-T} * P @f$
/// - Tangent: Applies @f$ P \to normalize(M * P) @f$
/// - Bitangent: Applies @f$ P \to normalize(M * P) @f$
///
/// @todo          Add an overload for 2D transforms.
///
/// @param[in,out] mesh       Mesh to transform in-place.
/// @param[in]     transform  Affine transform to apply.
/// @param[in]     options    Transform options.
///
/// @tparam        Scalar     Mesh scalar type.
/// @tparam        Index      Mesh index type.
/// @tparam        Dimension  Transform dimension (either 2 or 3). Must match mesh dimension.
///
template <typename Scalar, typename Index, int Dimension>
void transform_mesh(
    SurfaceMesh<Scalar, Index>& mesh,
    const Eigen::Transform<Scalar, Dimension, Eigen::Affine>& transform,
    const TransformOptions& options = {});

///
/// Apply an affine transform to a mesh and return the transformed mesh. All mesh attributes are
/// transformed based on their usage tags:
///
/// - Position: Applies @f$ P \to M * P @f$
/// - Normal: Applies @f$ P \to \det(M) M^{-T} * P @f$
/// - Tangent: Applies @f$ P \to normalize(M * P) @f$
/// - Bitangent: Applies @f$ P \to normalize(M * P) @f$
///
/// @todo       Add an overload for 2D transforms.
///
/// @param[in]  mesh       Mesh to transform in-place.
/// @param[in]  transform  Affine transform to apply.
/// @param[in]  options    Transform options.
///
/// @tparam     Scalar     Mesh scalar type.
/// @tparam     Index      Mesh index type.
/// @tparam     Dimension  Transform dimension (either 2 or 3). Must match mesh dimension.
///
/// @return     Transformed mesh.
///
template <typename Scalar, typename Index, int Dimension>
SurfaceMesh<Scalar, Index> transformed_mesh(
    SurfaceMesh<Scalar, Index> mesh,
    const Eigen::Transform<Scalar, Dimension, Eigen::Affine>& transform,
    const TransformOptions& options = {});

///
/// @}
///

} // namespace lagrange

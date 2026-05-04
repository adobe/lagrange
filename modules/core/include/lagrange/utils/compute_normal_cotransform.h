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

#include <Eigen/Core>
#include <Eigen/Geometry>

namespace lagrange {

/// @addtogroup group-utils
/// @{

///
/// Computes the normal (cotransform) matrix for a 3D affine transform.
///
/// For transforming normals correctly under an affine transform M, one should use det(M) * M^{-T},
/// which equals the cofactor matrix of the linear part of M.
///
/// @param[in]  transform  Input 3D affine transform. Since the translation part does not affect
///                        normal transformation, it is ignored and only the linear part is used.
///
/// @tparam     Scalar     Scalar type.
///
/// @return     A 3x3 matrix for transforming normals.
///
/// @see        transform_mesh() for usage with mesh normal attributes.
///
template <typename Scalar>
Eigen::Matrix3<Scalar> compute_normal_cotransform(
    const Eigen::Transform<Scalar, 3, Eigen::Affine>& transform)
{
    const auto& matrix = transform.linear();
    auto minor2x2 = [&](int i, int j) -> Scalar {
        const int i1 = (i == 0 ? 1 : 0);
        const int i2 = (i == 2 ? 1 : 2);
        const int j1 = (j == 0 ? 1 : 0);
        const int j2 = (j == 2 ? 1 : 2);
        return matrix(i1, j1) * matrix(i2, j2) - matrix(i1, j2) * matrix(i2, j1);
    };

    Eigen::Matrix3<Scalar> result;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            result(i, j) = ((i + j) % 2 == 0 ? Scalar(1) : Scalar(-1)) * minor2x2(i, j);
        }
    }
    return result;
}

///
/// Computes the normal (cotransform) matrix for a 2D affine transform.
///
/// @param[in]  transform  Input 2D affine transform. Since the translation part does not affect
///                        normal transformation, it is ignored and only the linear part is used.
///
/// @tparam     Scalar     Scalar type.
///
/// @return     A 2x2 matrix for transforming normals.
///
template <typename Scalar>
Eigen::Matrix2<Scalar> compute_normal_cotransform(
    const Eigen::Transform<Scalar, 2, Eigen::Affine>& transform)
{
    const auto& matrix = transform.linear();
    Eigen::Matrix2<Scalar> result;
    result(0, 0) = matrix(1, 1);
    result(0, 1) = -matrix(1, 0);
    result(1, 0) = -matrix(0, 1);
    result(1, 1) = matrix(0, 0);
    return result;
}

/// @}

} // namespace lagrange

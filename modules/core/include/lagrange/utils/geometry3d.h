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

#include <Eigen/Dense>
#include <lagrange/MeshTrait.h>

namespace lagrange {

/// Returns the cosine of the angle between two 3d vectors.
///
/// Assumes both vectors are normalized (unit vector).
template <typename Scalar, int _Rows, int _Cols>
Scalar cos_angle_between(
    const Eigen::Matrix<Scalar, _Rows, _Cols>& v1,
    const Eigen::Matrix<Scalar, _Rows, _Cols>& v2)
{
    static_assert((_Rows == 1 && _Cols == 3) || (_Rows == 3 && _Cols == 1), "");
    Scalar dot = v1.dot(v2);
    if (dot < -1) return Scalar(-1.0);
    if (dot > 1) return Scalar(1.0);
    return dot;
}

/// Returns the angle between two 3d vectors.
template <typename Scalar, int _Rows, int _Cols>
Scalar angle_between(
    const Eigen::Matrix<Scalar, _Rows, _Cols>& v1,
    const Eigen::Matrix<Scalar, _Rows, _Cols>& v2)
{
    static_assert((_Rows == 1 && _Cols == 3) || (_Rows == 3 && _Cols == 1), "");
    return std::atan2(v1.cross(v2).norm(), v1.dot(v2));
}

/// Project the vector v1 on the line defined by its vector v2
///
/// Assumes the vector v2 is normalized (unit vector).
template <typename Scalar, int _Rows, int _Cols>
Eigen::Matrix<Scalar, _Rows, _Cols> project_on_line(
    const Eigen::Matrix<Scalar, _Rows, _Cols>& v1,
    const Eigen::Matrix<Scalar, _Rows, _Cols>& v2)
{
    static_assert((_Rows == 1 && _Cols == 3) || (_Rows == 3 && _Cols == 1), "");
    return v1.dot(v2) * v2;
}

/// Project the vector on the plane defined by its normal n.
/// Assumes the normal n is a unit vector.
template <typename Scalar, int _Rows, int _Cols>
Eigen::Matrix<Scalar, _Rows, _Cols> project_on_plane(
    const Eigen::Matrix<Scalar, _Rows, _Cols>& v,
    const Eigen::Matrix<Scalar, _Rows, _Cols>& n)
{
    static_assert((_Rows == 1 && _Cols == 3) || (_Rows == 3 && _Cols == 1), "");
    return v - project_on_line(v, n);
}

/// Returns the angle between the vectors v1 and v2 projected on the plane defined
/// by its normal n. Assumes the normal n is a unit vector.
template <typename Scalar, int _Rows, int _Cols>
Scalar projected_cos_angle_between(
    const Eigen::Matrix<Scalar, _Rows, _Cols>& v1,
    const Eigen::Matrix<Scalar, _Rows, _Cols>& v2,
    const Eigen::Matrix<Scalar, _Rows, _Cols>& n)
{
    static_assert((_Rows == 1 && _Cols == 3) || (_Rows == 3 && _Cols == 1), "");
    const Eigen::Matrix<Scalar, _Rows, _Cols> proj1 = project_on_plane(v1, n).stableNormalized();
    const Eigen::Matrix<Scalar, _Rows, _Cols> proj2 = project_on_plane(v2, n).stableNormalized();
    return cos_angle_between(proj1, proj2);
}

/// Returns the angle between the vectors v1 and v2 projected on the plane defined
/// by its normal n. Assumes the normal n is a unit vector.
template <typename Scalar, int _Rows, int _Cols>
Scalar projected_angle_between(
    const Eigen::Matrix<Scalar, _Rows, _Cols>& v1,
    const Eigen::Matrix<Scalar, _Rows, _Cols>& v2,
    const Eigen::Matrix<Scalar, _Rows, _Cols>& n)
{
    static_assert((_Rows == 1 && _Cols == 3) || (_Rows == 3 && _Cols == 1), "");
    const Eigen::Matrix<Scalar, _Rows, _Cols> proj1 = project_on_plane(v1, n);
    const Eigen::Matrix<Scalar, _Rows, _Cols> proj2 = project_on_plane(v2, n);
    return std::atan2(proj1.cross(proj2).norm(), proj1.dot(proj2));
}

///
/// Returns the vector from v1 to v2
///
/// @param[in] v1 first vertex index (from).
/// @param[in] v2 second vertex index (to)
///
/// @return The 3D vector
///
template <typename MeshType>
auto vector_between(const MeshType& mesh, typename MeshType::Index v1, typename MeshType::Index v2)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "MeshType is not a mesh");
    return mesh.get_vertices().row(v2) - mesh.get_vertices().row(v1);
}

///
/// Build an orthogonal frame given a single vector.
///
/// @param[in]  x       First vector of the frame.
/// @param[out] y       Second vector of the frame.
/// @param[out] z       Third vector of the frame.
///
/// @tparam     Scalar  Scalar type.
///
template <typename Scalar>
void orthogonal_frame(
    const Eigen::Matrix<Scalar, 3, 1>& x,
    Eigen::Matrix<Scalar, 3, 1>& y,
    Eigen::Matrix<Scalar, 3, 1>& z)
{
    int imin;
    x.array().abs().minCoeff(&imin);
    Eigen::Matrix<Scalar, 3, 1> u;
    for (int i = 0, s = -1; i < 3; ++i) {
        if (i == imin) {
            u[i] = 0;
        } else {
            int j = (i + 1) % 3;
            if (j == imin) {
                j = (i + 2) % 3;
            }
            u[i] = s * x[j];
            s *= -1;
        }
    }
    z = x.cross(u).stableNormalized();
    y = z.cross(x).stableNormalized();
}

} // namespace lagrange

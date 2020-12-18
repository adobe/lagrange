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

namespace lagrange {

/// Returns the cosine of the angle between two 3d vectors.
///
/// Assumes both vectors are normalized (unit vector).
template <typename Scalar>
Scalar cos_angle_between(
    const Eigen::Matrix<Scalar, 3, 1>& v1,
    const Eigen::Matrix<Scalar, 3, 1>& v2)
{
    Scalar dot = v1.dot(v2);
    if (dot < -1) return Scalar(-1.0);
    if (dot > 1) return Scalar(1.0);
    return dot;
}

template <typename Scalar>
Scalar cos_angle_between(
    const Eigen::Matrix<Scalar, 1, 3>& v1,
    const Eigen::Matrix<Scalar, 1, 3>& v2)
{
    Scalar dot = v1.dot(v2);
    if (dot < -1) return Scalar(-1.0);
    if (dot > 1) return Scalar(1.0);
    return dot;
}

/// Returns the angle between two 3d vectors.
template <typename Scalar>
Scalar angle_between(const Eigen::Matrix<Scalar, 3, 1>& v1, const Eigen::Matrix<Scalar, 3, 1>& v2)
{
    return std::atan2(v1.cross(v2).norm(), v1.dot(v2));
}

template <typename Scalar>
Scalar angle_between(const Eigen::Matrix<Scalar, 1, 3>& v1, const Eigen::Matrix<Scalar, 1, 3>& v2)
{
    return std::atan2(v1.cross(v2).norm(), v1.dot(v2));
}

/// Project the vector v1 on the line defined by its vector v2
///
/// Assumes the vector v2 is normalized (unit vector).
template <typename Scalar>
Eigen::Matrix<Scalar, 3, 1> project_on_line(
    const Eigen::Matrix<Scalar, 3, 1>& v1,
    const Eigen::Matrix<Scalar, 3, 1>& v2)
{
    return v1.dot(v2) * v2;
}

template <typename Scalar>
Eigen::Matrix<Scalar, 1, 3> project_on_line(
    const Eigen::Matrix<Scalar, 1, 3>& v1,
    const Eigen::Matrix<Scalar, 1, 3>& v2)
{
    return v1.dot(v2) * v2;
}

/// Project the vector on the plane defined by its normal n.
/// Assumes the normal n is a unit vector.
template <typename Scalar>
Eigen::Matrix<Scalar, 3, 1> project_on_plane(
    const Eigen::Matrix<Scalar, 3, 1>& v,
    const Eigen::Matrix<Scalar, 3, 1>& n)
{
    return v - project_on_line(v, n);
}

template <typename Scalar>
Eigen::Matrix<Scalar, 1, 3> project_on_plane(
    const Eigen::Matrix<Scalar, 1, 3>& v,
    const Eigen::Matrix<Scalar, 1, 3>& n)
{
    return v - project_on_line(v, n);
}

/// Returns the angle between the vectors v1 and v2 projected on the plane defined
/// by its normal n. Assumes the normal n is a unit vector.
template <typename Scalar>
Scalar projected_cos_angle_between(
    const Eigen::Matrix<Scalar, 3, 1>& v1,
    const Eigen::Matrix<Scalar, 3, 1>& v2,
    const Eigen::Matrix<Scalar, 3, 1>& n)
{
    const Eigen::Matrix<Scalar, 3, 1> proj1 = project_on_plane(v1, n).stableNormalized();
    const Eigen::Matrix<Scalar, 3, 1> proj2 = project_on_plane(v2, n).stableNormalized();
    return cos_angle_between(proj1, proj2);
}

template <typename Scalar>
Scalar projected_cos_angle_between(
    const Eigen::Matrix<Scalar, 1, 3>& v1,
    const Eigen::Matrix<Scalar, 1, 3>& v2,
    const Eigen::Matrix<Scalar, 1, 3>& n)
{
    const Eigen::Matrix<Scalar, 1, 3> proj1 = project_on_plane(v1, n).stableNormalized();
    const Eigen::Matrix<Scalar, 1, 3> proj2 = project_on_plane(v2, n).stableNormalized();
    return cos_angle_between(proj1, proj2);
}

/// Returns the angle between the vectors v1 and v2 projected on the plane defined
/// by its normal n. Assumes the normal n is a unit vector.
template <typename Scalar>
Scalar projected_angle_between(
    const Eigen::Matrix<Scalar, 3, 1>& v1,
    const Eigen::Matrix<Scalar, 3, 1>& v2,
    const Eigen::Matrix<Scalar, 3, 1>& n)
{
    const Eigen::Matrix<Scalar, 3, 1> proj1 = project_on_plane(v1, n);
    const Eigen::Matrix<Scalar, 3, 1> proj2 = project_on_plane(v2, n);
    return std::atan2(proj1.cross(proj2).norm(), proj1.dot(proj2));
}

template <typename Scalar>
Scalar projected_angle_between(
    const Eigen::Matrix<Scalar, 1, 3>& v1,
    const Eigen::Matrix<Scalar, 1, 3>& v2,
    const Eigen::Matrix<Scalar, 1, 3>& n)
{
    const Eigen::Matrix<Scalar, 1, 3> proj1 = project_on_plane(v1, n);
    const Eigen::Matrix<Scalar, 1, 3> proj2 = project_on_plane(v2, n);
    return std::atan2(proj1.cross(proj2).norm(), proj1.dot(proj2));
}

} // namespace lagrange

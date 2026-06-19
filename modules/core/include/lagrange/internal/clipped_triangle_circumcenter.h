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

#include <lagrange/utils/assert.h>

#include <Eigen/Core>

#include <cmath>
#include <limits>

namespace lagrange::internal {

///
/// Compute the circumcenter of a triangle (p1, p2, p3), clipped to the triangle if it lies
/// outside. Barycentric coordinates of the returned point are written to lambda1, lambda2,
/// lambda3 (each in [0, 1], summing to 1).
///
template <typename Scalar>
Eigen::Vector3<Scalar> clipped_triangle_circumcenter(
    const Eigen::Vector3<Scalar>& p1,
    const Eigen::Vector3<Scalar>& p2,
    const Eigen::Vector3<Scalar>& p3,
    Scalar& lambda1,
    Scalar& lambda2,
    Scalar& lambda3)
{
    [[maybe_unused]] constexpr const Scalar epsilon = 1e-7f;

    using Vec3 = Eigen::Vector3<Scalar>;

    const Vec3 q2 = p2 - p1;
    const Vec3 q3 = p3 - p1;

    Scalar l2 = q2.squaredNorm();
    Scalar l3 = q3.squaredNorm();

    Scalar a12 = -2.0f * q2.dot(q2);
    Scalar a13 = -2.0f * q3.dot(q2);
    Scalar a22 = -2.0f * q2.dot(q3);
    Scalar a23 = -2.0f * q3.dot(q3);

    Scalar c31 = (a23 * a12 - a22 * a13);
    Scalar d = c31;
    if (std::abs(d) < std::numeric_limits<Scalar>::denorm_min()) {
        // Degenerate (collinear) triangle: the circumcenter is undefined. Fall back to the
        // triangle barycenter.
        lambda1 = lambda2 = lambda3 = Scalar(1) / Scalar(3);
        return lambda1 * p1 + lambda2 * p2 + lambda3 * p3;
    }
    Scalar s = 1.0f / d;
    lambda1 = s * ((a23 - a22) * l2 + (a12 - a13) * l3 + c31);
    lambda2 = s * ((-a23) * l2 + (a13)*l3);
    lambda3 = s * ((a22)*l2 + (-a12) * l3);

    if (lambda1 < 0) {
        lambda1 = 0;
        la_debug_assert(lambda2 >= 0);
        la_debug_assert(lambda3 >= 0);
        lambda2 = 0.5f;
        lambda3 = 0.5f;
    }

    if (lambda2 < 0) {
        lambda2 = 0;
        la_debug_assert(lambda1 >= 0);
        la_debug_assert(lambda3 >= 0);
        lambda1 = 0.5f;
        lambda3 = 0.5f;
    }

    if (lambda3 < 0) {
        lambda3 = 0;
        la_debug_assert(lambda1 >= 0);
        la_debug_assert(lambda2 >= 0);
        lambda1 = 0.5f;
        lambda2 = 0.5f;
    }

    la_debug_assert(std::fabs(lambda1 + lambda2 + lambda3 - 1) < epsilon);
    return lambda1 * p1 + lambda2 * p2 + lambda3 * p3;
}

} // namespace lagrange::internal

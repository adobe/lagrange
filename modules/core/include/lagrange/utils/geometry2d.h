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

#include <Eigen/Core>

namespace lagrange {

/// Returns the squared minimum distance between 2d line segment ab and point p
template <typename Scalar>
Scalar sqr_minimum_distance(
    const Eigen::Matrix<Scalar, 2, 1>& a,
    const Eigen::Matrix<Scalar, 2, 1>& b,
    const Eigen::Matrix<Scalar, 2, 1>& p)
{
    using Vec2 = Eigen::Matrix<Scalar, 2, 1>;

    const Scalar l2 = (a - b).squaredNorm();
    if (l2 == 0.0) return (p - a).squaredNorm(); // a==b

    Scalar t = (p - a).dot(b - a) / l2;
    if (t < 0) t = Scalar(0);
    if (t > 1) t = Scalar(1);
    const Vec2 projection = a + t * (b - a);
    return (p - projection).squaredNorm();
}

} // namespace lagrange

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
    const Eigen::Vector2<Scalar>& a,
    const Eigen::Vector2<Scalar>& b,
    const Eigen::Vector2<Scalar>& p)
{
    const Scalar l2 = (a - b).squaredNorm();
    if (l2 == 0.0) return (p - a).squaredNorm(); // a==b

    Scalar t = (p - a).dot(b - a) / l2;
    if (t < 0) t = Scalar(0);
    if (t > 1) t = Scalar(1);
    const Eigen::Vector2<Scalar> projection = a + t * (b - a);
    return (p - projection).squaredNorm();
}

template <typename Scalar>
std::array<Scalar, 2> triangle_circumcenter_2d(
    span<const Scalar, 2> p1,
    span<const Scalar, 2> p2,
    span<const Scalar, 2> p3)
{
    Scalar a = p2[0] - p1[0];
    Scalar b = p2[1] - p1[1];
    Scalar c = p3[0] - p1[0];
    Scalar d = p3[1] - p1[1];
    Scalar e = a * (p1[0] + p2[0]) + b * (p1[1] + p2[1]);
    Scalar f = c * (p1[0] + p3[0]) + d * (p1[1] + p3[1]);
    Scalar g = 2 * (a * (p3[1] - p2[1]) - b * (p3[0] - p2[0]));

    if (std::abs(g) < std::numeric_limits<Scalar>::epsilon()) {
        Scalar minx = std::min({p1[0], p2[0], p3[0]});
        Scalar miny = std::min({p1[1], p2[1], p3[1]});
        Scalar dx = (std::max({p1[0], p2[0], p3[0]}) - minx) * 0.5f;
        Scalar dy = (std::max({p1[1], p2[1], p3[1]}) - miny) * 0.5f;

        return {minx + dx, miny + dy};
    } else {
        return {(d * e - b * f) / g, (a * f - c * e) / g};
    }
}

// Returns the circumcenter of a 2D triangle.
template <typename Scalar>
Eigen::Vector2<Scalar> triangle_circumcenter_2d(
    const Eigen::Vector2<Scalar>& p1,
    const Eigen::Vector2<Scalar>& p2,
    const Eigen::Vector2<Scalar>& p3)
{
    auto r = triangle_circumcenter_2d<Scalar>(
        span<const Scalar, 2>({p1.x(), p1.y()}),
        span<const Scalar, 2>({p2.x(), p2.y()}),
        span<const Scalar, 2>({p3.x(), p3.y()}));
    return Eigen::Vector2<Scalar>(r[0], r[1]);
}


template <typename Scalar>
[[deprecated]]
Eigen::Vector2<Scalar> triangle_circumcenter(
    const Eigen::Vector2<Scalar>& p1,
    const Eigen::Vector2<Scalar>& p2,
    const Eigen::Vector2<Scalar>& p3)
{
    auto r = triangle_circumcenter_2d<Scalar>({p1.x(), p1.y()}, {p2.x(), p2.y()}, {p3.x(), p3.y()});
    return Eigen::Vector2<Scalar>(r[0], r[1]);
}

} // namespace lagrange

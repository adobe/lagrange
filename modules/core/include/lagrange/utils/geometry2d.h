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

// Returns the circumcenter of a 2D triangle.
template <typename Scalar>
Eigen::Vector2<Scalar> triangle_circumcenter(
    const Eigen::Vector2<Scalar>& p1,
    const Eigen::Vector2<Scalar>& p2,
    const Eigen::Vector2<Scalar>& p3)
{
    Scalar a = p2.x() - p1.x();
    Scalar b = p2.y() - p1.y();
    Scalar c = p3.x() - p1.x();
    Scalar d = p3.y() - p1.y();
    Scalar e = a * (p1.x() + p2.x()) + b * (p1.y() + p2.y());
    Scalar f = c * (p1.x() + p3.x()) + d * (p1.y() + p3.y());
    Scalar g = 2 * (a * (p3.y() - p2.y()) - b * (p3.x() - p2.x()));

    if (std::abs(g) < std::numeric_limits<Scalar>::epsilon()) {
        Scalar minx = std::min({p1.x(), p2.x(), p3.x()});
        Scalar miny = std::min({p1.y(), p2.y(), p3.y()});
        Scalar dx = (std::max({p1.x(), p2.x(), p3.x()}) - minx) * 0.5f;
        Scalar dy = (std::max({p1.y(), p2.y(), p3.y()}) - miny) * 0.5f;

        return Eigen::Vector2<Scalar>(minx + dx, miny + dy);
    } else {
        Eigen::Vector2<Scalar> center((d * e - b * f) / g, (a * f - c * e) / g);
        return center;
    }
}

} // namespace lagrange

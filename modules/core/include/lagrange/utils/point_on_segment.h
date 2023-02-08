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

namespace internal {

/// @internal
bool point_on_segment_2d(Eigen::Vector2d p, Eigen::Vector2d a, Eigen::Vector2d b);

/// @internal
bool point_on_segment_3d(Eigen::Vector3d p, Eigen::Vector3d a, Eigen::Vector3d b);

}

///
/// Test if a point lies exactly on a segment [a,b] using exact predicates. If the points are
/// collinear, each individual coordinate is examined to determine if the query point lies inside
/// the segment or outside of it.
///
/// @param[in]  p          Query point.
/// @param[in]  a          First segment endpoint.
/// @param[in]  b          Second segment endpoint.
///
/// @tparam     PointType  Point type.
///
/// @return     True if the query point lies exactly on the segment, False otherwise.
///
template <typename PointType>
bool point_on_segment(
    const Eigen::MatrixBase<PointType>& p,
    const Eigen::MatrixBase<PointType>& a,
    const Eigen::MatrixBase<PointType>& b)
{
    if (p.size() == 2 && a.size() == 2 && b.size() == 2) {
        Eigen::Vector2d p2d(static_cast<double>(p.x()), static_cast<double>(p.y()));
        Eigen::Vector2d a2d(static_cast<double>(a.x()), static_cast<double>(a.y()));
        Eigen::Vector2d b2d(static_cast<double>(b.x()), static_cast<double>(b.y()));
        return internal::point_on_segment_2d(p2d, a2d, b2d);
    } else if (p.size() == 3 && a.size() == 3 && b.size() == 3) {
        Eigen::Vector3d p3d(
            static_cast<double>(p[0]),
            static_cast<double>(p[1]),
            static_cast<double>(p[2]));
        Eigen::Vector3d a3d(
            static_cast<double>(a[0]),
            static_cast<double>(a[1]),
            static_cast<double>(a[2]));
        Eigen::Vector3d b3d(
            static_cast<double>(b[0]),
            static_cast<double>(b[1]),
            static_cast<double>(b[2]));
        return internal::point_on_segment_3d(p3d, a3d, b3d);
    } else {
        throw std::runtime_error("Incompatible types");
    }
}

} // namespace lagrange

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
#include <lagrange/utils/point_on_segment.h>

#include <lagrange/ExactPredicatesShewchuk.h>

namespace lagrange {

namespace internal {

bool point_on_segment_2d(Eigen::Vector2d p, Eigen::Vector2d a, Eigen::Vector2d b)
{
    ExactPredicatesShewchuk pred;
    auto res = pred.orient2D(p.data(), a.data(), b.data());
    if (res != 0) {
        return false;
    }
    if (a.x() > b.x()) {
        std::swap(a.x(), b.x());
    }
    if (a.y() > b.y()) {
        std::swap(a.y(), b.y());
    }
    auto ret = (a.x() <= p.x() && p.x() <= b.x() && a.y() <= p.y() && p.y() <= b.y());
    return ret;
}

bool point_on_segment_3d(Eigen::Vector3d p, Eigen::Vector3d a, Eigen::Vector3d b)
{
    for (int d = 0; d < 3; ++d) {
        Eigen::Vector2d p2d(p(d), p((d + 1) % 3));
        Eigen::Vector2d a2d(a(d), a((d + 1) % 3));
        Eigen::Vector2d b2d(b(d), b((d + 1) % 3));
        if (!point_on_segment_2d(p2d, a2d, b2d)) {
            return false;
        }
    }
    return true;
}

} // namespace internal
} // namespace lagrange

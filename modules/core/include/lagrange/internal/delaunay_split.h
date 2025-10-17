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

#include <lagrange/internal/constants.h>
#include <lagrange/utils/geometry3d.h>

#include <array>
#include <cstdint>

namespace lagrange::internal {

template <typename Derived>
std::array<std::array<uint8_t, 3>, 2> delaunay_split(
    const Eigen::DenseBase<Derived>& p0,
    const Eigen::DenseBase<Derived>& p1,
    const Eigen::DenseBase<Derived>& p2,
    const Eigen::DenseBase<Derived>& p3)
{
    EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Derived, 3);
    // Choose split that maximizes the minimum angle
    using Scalar = typename Derived::Scalar;
    using Vector3s = Eigen::Vector3<Scalar>;
    auto angle1 =
        angle_between(Vector3s(p0.derived() - p1.derived()), Vector3s(p2.derived() - p1.derived()));
    auto angle2 =
        angle_between(Vector3s(p0.derived() - p3.derived()), Vector3s(p2.derived() - p3.derived()));
    if (angle1 + angle2 <= lagrange::internal::pi) {
        return {{{0, 1, 2}, {0, 2, 3}}};
    } else {
        return {{{0, 1, 3}, {1, 2, 3}}};
    }
}

} // namespace lagrange::internal

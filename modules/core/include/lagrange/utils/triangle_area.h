/*
 * Copyright 2023 Adobe. All rights reserved.
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
#include <lagrange/utils/span.h>

#include <cmath>

namespace lagrange {

/**
 * Compute 3D triangle area.
 *
 * @tparam Scalar  The scalar type.
 *
 * @param a,b,c  Triangle vertex coordinates in counterclockwise order.
 *
 * @return The area of the triangle.
 */
template <typename Scalar>
Scalar triangle_area_3d(span<const Scalar, 3> a, span<const Scalar, 3> b, span<const Scalar, 3> c)
{
    const Scalar n0 = (a[1] - b[1]) * (a[2] - c[2]) - (a[1] - c[1]) * (a[2] - b[2]);
    const Scalar n1 = -(a[0] - b[0]) * (a[2] - c[2]) + (a[0] - c[0]) * (a[2] - b[2]);
    const Scalar n2 = (a[0] - b[0]) * (a[1] - c[1]) - (a[0] - c[0]) * (a[1] - b[1]);

    return std::sqrt(n0 * n0 + n1 * n1 + n2 * n2) / 2;
}

/**
 * Compute 2D triangle signed area.
 *
 * @tparam Scalar  The scalar type.
 *
 * @param a,b,c  Triangle vertex coordinates in counterclockwise order.
 *
 * @return The signed area of the triangle.
 */
template <typename Scalar>
Scalar triangle_area_2d(span<const Scalar, 2> a, span<const Scalar, 2> b, span<const Scalar, 2> c)
{
    return ((a[0] - b[0]) * (a[1] - c[1]) - (a[0] - c[0]) * (a[1] - b[1])) / 2;
}

} // namespace lagrange

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
#include <lagrange/utils/triangle_area.h>

namespace lagrange {

/**
 * Compute 2D quad signed area.
 *
 * @tparam Scalar  The scalar type.
 *
 * @param a,b,c,d  Quad vertex coordinates in counterclockwise order.
 *
 * @return The signed area of the quad.
 */
template <typename Scalar>
Scalar quad_area_2d(
    span<const Scalar, 2> a,
    span<const Scalar, 2> b,
    span<const Scalar, 2> c,
    span<const Scalar, 2> d)
{
    Scalar _center[2]{(a[0] + b[0] + c[0] + d[0]) / 4, (a[1] + b[1] + c[1] + d[1]) / 4};
    span<const Scalar, 2> center(_center, 2);
    return triangle_area_2d(a, b, center) + triangle_area_2d(b, c, center) +
           triangle_area_2d(c, d, center) + triangle_area_2d(d, a, center);
}

/**
 * Compute 3D quad area.
 *
 * @tparam Scalar  The scalar type.
 *
 * @param a,b,c,d  Quad vertex coordinates in counterclockwise order.
 *
 * @return The area of the quad.
 */
template <typename Scalar>
Scalar quad_area_3d(
    span<const Scalar, 3> a,
    span<const Scalar, 3> b,
    span<const Scalar, 3> c,
    span<const Scalar, 3> d)
{
    Scalar _center[3]{
        (a[0] + b[0] + c[0] + d[0]) / 4,
        (a[1] + b[1] + c[1] + d[1]) / 4,
        (a[2] + b[2] + c[2] + d[2]) / 4};
    span<const Scalar, 3> center(_center, 3);
    return triangle_area_3d(a, b, center) + triangle_area_3d(b, c, center) +
           triangle_area_3d(c, d, center) + triangle_area_3d(d, a, center);
}

} // namespace lagrange

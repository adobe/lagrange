/*
 * Copyright 2026 Adobe. All rights reserved.
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
#include <Eigen/Geometry>

namespace lagrange::texproc::internal {

///
/// Stack-allocated matrix for storing vertices of a 2D polygon when its maximum size is known
/// in advance.
///
/// @tparam Scalar The scalar type of vertices.
/// @tparam Size   The maximum number of vertices.
///
template <typename Scalar, int Size>
using SmallPolygon2 = Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor, Size, 2>;

///
/// Clip a triangle by an axis-aligned box.
///
/// @param[in]  triangle   Triangle to clip.
/// @param[in]  bbox       Axis-aligned bbox to clip with.
///
/// @tparam     Scalar     The scalar type of vertices.
///
/// @return     Clipped (convex) polygon.
///
template <typename Scalar>
SmallPolygon2<Scalar, 7> clip_triangle_by_bbox(
    const SmallPolygon2<Scalar, 3>& triangle,
    const Eigen::AlignedBox<Scalar, 2>& bbox);

} // namespace lagrange::texproc::internal

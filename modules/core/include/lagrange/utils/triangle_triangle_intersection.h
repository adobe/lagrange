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

#include <lagrange/api.h>
#include <lagrange/utils/span.h>

namespace lagrange {

///
/// @defgroup group-utils-geom Geometry utilities
/// @ingroup group-utils
/// @brief Geometric predicates and primitive intersection tests.
/// @{
///

///
/// Controls whether boundary contact counts as intersection in triangle_triangle_intersection().
///
enum class IncludeBoundaryIntersection {
    /// Only interior intersections count. Triangles touching at vertices or edges are not
    /// considered intersecting.
    No,

    /// Any contact counts as intersection, including touching at vertices or edges.
    Yes
};

///
/// Check if two 3D triangles intersect using exact predicates for robustness.
///
/// This function uses Shewchuk's exact orient3D and orient2D predicates to robustly determine
/// if two triangles intersect in 3D space. The algorithm tests each edge of one triangle
/// against the other triangle using tetrahedra orientation tests, with special handling for
/// coplanar cases using 2D projections.
///
/// @param[in] t1_v0    First vertex of triangle 1.
/// @param[in] t1_v1    Second vertex of triangle 1.
/// @param[in] t1_v2    Third vertex of triangle 1.
/// @param[in] t2_v0    First vertex of triangle 2.
/// @param[in] t2_v1    Second vertex of triangle 2.
/// @param[in] t2_v2    Third vertex of triangle 2.
/// @param[in] boundary Whether touching at boundaries counts as intersection.
///
/// @tparam Scalar      The scalar type (e.g., float, double).
///
/// @return             True if the triangles intersect, false otherwise.
///
/// @note Orientation and coplanarity decisions rely on Shewchuk's exact predicates for
///       robustness. Some auxiliary computations (e.g., projection axis selection and
///       collinear interval parameterization) use standard floating-point arithmetic.
///
template <typename Scalar>
bool triangle_triangle_intersection(
    span<const Scalar, 3> t1_v0,
    span<const Scalar, 3> t1_v1,
    span<const Scalar, 3> t1_v2,
    span<const Scalar, 3> t2_v0,
    span<const Scalar, 3> t2_v1,
    span<const Scalar, 3> t2_v2,
    IncludeBoundaryIntersection boundary = IncludeBoundaryIntersection::No);

/// @}

} // namespace lagrange

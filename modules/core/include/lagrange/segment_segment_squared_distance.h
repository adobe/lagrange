/*
 * Copyright 2022 Adobe. All rights reserved.
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

#include <lagrange/common.h>
#include <Eigen/Dense>
#include <algorithm>
#include <cmath>

namespace lagrange {

///
/// Computes the squared distance between two N-d line segments, and the closest pair of points
/// whose separation is this distance.
///
/// @param[in]  U0              first extremity of the first segment
/// @param[in]  U1              second extremity of the first segment
/// @param[in]  V0              first extremity of the second segment
/// @param[in]  V1              second extremity of the second segment
/// @param[out] closest_pointU  the closest point on segment [@p U0, @p U1]
/// @param[out] closest_pointV  the closest point on segment [@p V0, @p V1]
/// @param[out] lambdaU         barycentric coordinate of the closest point relative to [@p U0, U1]
/// @param[out] lambdaV         barycentric coordinate of the closest point relative to [@p V0, V1]
///
/// @tparam     PointType       the class that represents the points.
///
/// @return     the squared distance between the segments [@p U0, @p U1] and [@p V0, @p V1]
///
/// @note Adapted from Real-Time Collision Detection by Christer Ericson, published by Morgan
///   Kaufmann Publishers, (c) 2005 Elsevier Inc. This function was modified for use by Siddhartha
///   Chaudhuri in the Thea library, from where this version was taken on 28 Sep 2022. All
///   modifications beyond Thea are Copyright 2022 Adobe.
///
template <typename PointType>
auto segment_segment_squared_distance(
    const Eigen::MatrixBase<PointType>& U0,
    const Eigen::MatrixBase<PointType>& U1,
    const Eigen::MatrixBase<PointType>& V0,
    const Eigen::MatrixBase<PointType>& V1,
    Eigen::PlainObjectBase<PointType>& closest_pointU,
    Eigen::PlainObjectBase<PointType>& closest_pointV,
    ScalarOf<PointType>& lambdaU,
    ScalarOf<PointType>& lambdaV) -> ScalarOf<PointType>
{
    using Scalar = ScalarOf<PointType>;
    using Vector = typename PointType::PlainObject;

    constexpr Scalar ZERO = static_cast<Scalar>(0);
    constexpr Scalar ONE = static_cast<Scalar>(1);
    constexpr Scalar EPS = std::numeric_limits<Scalar>::epsilon(); // is this the best choice?

    // Expose these in the function signature if we need to support unbounded lines as well
    constexpr bool is_lineU = false;
    constexpr bool is_lineV = false;

    Vector d1 = U1 - U0; // Direction vector of segment U
    Vector d2 = V1 - V0; // Direction vector of segment V
    Vector r = U0 - V0;
    Scalar a = d1.squaredNorm(); // Squared length of segment U, always nonnegative
    Scalar e = d2.squaredNorm(); // Squared length of segment V, always nonnegative
    Scalar f = d2.dot(r);

    // Check if either or both segments degenerate into points
    if (std::abs(a) < EPS) {
        if (std::abs(e) < EPS) {
            // Both segments degenerate into points
            lambdaU = lambdaV = 0;
            closest_pointU = U0;
            closest_pointV = V0;
            return (closest_pointU - closest_pointV).dot(closest_pointU - closest_pointV);
        } else {
            // First segment degenerates into a point
            lambdaU = 0;
            lambdaV = f / e; // lambdaU = 0 => lambdaV = (b*lambdaU + f) / e = f / e

            if (!is_lineV) lambdaV = std::clamp(lambdaV, ZERO, ONE);
        }
    } else {
        Scalar c = d1.dot(r);
        if (std::abs(e) < EPS) {
            // Second segment degenerates into a point
            lambdaV = 0;
            lambdaU = -c / a;

            if (!is_lineU) // lambdaV = 0 => lambdaU = (b*lambdaV - c) / a = -c / a
                lambdaU = std::clamp(lambdaU, ZERO, ONE);
        } else {
            // The general nondegenerate case starts here
            Scalar b = d1.dot(d2);
            Scalar denom = a * e - b * b; // Always nonnegative

            // If segments not parallel, compute closest point on L1 to L2, and clamp to segment U.
            // Else pick arbitrary lambdaU (here 0)
            if (std::abs(denom) >= EPS) {
                lambdaU = (b * f - c * e) / denom;

                if (!is_lineU) lambdaU = std::clamp(lambdaU, ZERO, ONE);
            } else
                lambdaU = 0;

            // Compute point on L2 closest to U(lambdaU) using:
            // lambdaV = Dot((P1+D1*lambdaU)-P2,D2) / Dot(D2,D2) = (b*lambdaU + f) / e
            lambdaV = (b * lambdaU + f) / e;

            if (!is_lineV) {
                // If lambdaV in [0,1] done. Else clamp lambdaV, recompute lambdaU for the new value
                // of lambdaV using:
                // lambdaU = Dot((P2+D2*lambdaV)-P1,D1) / Dot(D1,D1)= (lambdaV*b - c) / a
                // and clamp lambdaU to [0, 1]
                if (lambdaV < 0) {
                    lambdaV = 0;
                    lambdaU = -c / a;

                    if (!is_lineU) lambdaU = std::clamp(lambdaU, ZERO, ONE);
                } else if (lambdaV > 1) {
                    lambdaV = 1;
                    lambdaU = (b - c) / a;

                    if (!is_lineU) lambdaU = std::clamp(lambdaU, ZERO, ONE);
                }
            }
        }
    }

    closest_pointU = U0 + lambdaU * d1;
    closest_pointV = V0 + lambdaV * d2;
    return (closest_pointU - closest_pointV).squaredNorm();
}

} // namespace lagrange

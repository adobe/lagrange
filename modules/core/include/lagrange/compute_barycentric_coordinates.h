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

#include <lagrange/utils/assert.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <Eigen/Dense>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <cmath>
#include <limits>

namespace lagrange {

///
/// Compute the barycentric coordinates of point @p p with respect to triangle (v0, v1, v2).
///
/// Uses a projection-based approach that solves a 2x2 Gram matrix system. This handles triangles in
/// any orientation, including those coplanar with a coordinate plane (e.g. z=0), triangles with a
/// vertex at the origin, and 2D points. For collinear or degenerate triangles, falls back to a
/// constrained least-squares solve with a partition-of-unity constraint (bary sums to 1).
///
/// @param[in]  v0  First triangle vertex.
/// @param[in]  v1  Second triangle vertex.
/// @param[in]  v2  Third triangle vertex.
/// @param[in]  p   Query point.
///
/// @tparam     PointType  Eigen column or row vector type.
///
/// @return     Barycentric coordinates (w0, w1, w2) such that p ≈ w0*v0 + w1*v1 + w2*v2.
///
template <typename PointType>
auto compute_barycentric_coordinates(
    const Eigen::MatrixBase<PointType>& v0,
    const Eigen::MatrixBase<PointType>& v1,
    const Eigen::MatrixBase<PointType>& v2,
    const Eigen::MatrixBase<PointType>& p) -> Eigen::Matrix<typename PointType::Scalar, 3, 1>
{
    using Scalar = typename PointType::Scalar;

    // Compile-time size info. For fully dynamic vectors (e.g. VectorXd), both are Dynamic and we
    // cap internal matrices at dim 3 to avoid heap allocation.
    constexpr int Dim = PointType::SizeAtCompileTime;
    constexpr int MaxDim = PointType::MaxSizeAtCompileTime;
    // Cap at 3 for stack allocation: use known max if available, otherwise 3.
    constexpr int EffectiveMaxDim = (MaxDim != Eigen::Dynamic) ? MaxDim : 3;
    constexpr int Rows = (Dim != Eigen::Dynamic) ? Dim + 1 : Eigen::Dynamic;
    constexpr int MaxRows = EffectiveMaxDim + 1;

    const Eigen::Index dim = p.size();
    la_debug_assert(dim <= EffectiveMaxDim && "Point dimension exceeds maximum supported size.");
    la_debug_assert(v0.size() == dim);
    la_debug_assert(v1.size() == dim);
    la_debug_assert(v2.size() == dim);

    // Fast path: solve 2x2 Gram matrix system via projection.
    //   e1 = v1 - v0,  e2 = v2 - v0,  ep = p - v0
    //   G = [e1·e1  e1·e2]    b = [ep·e1]
    //       [e1·e2  e2·e2]        [ep·e2]
    //   G * [u; v] = b  =>  p ≈ (1-u-v)*v0 + u*v1 + v*v2
    const auto e1 = (v1 - v0).eval();
    const auto e2 = (v2 - v0).eval();
    const auto ep = (p - v0).eval();

    Eigen::Matrix<Scalar, 2, 2> G;
    G(0, 0) = e1.squaredNorm();
    G(0, 1) = e1.dot(e2);
    G(1, 0) = G(0, 1);
    G(1, 1) = e2.squaredNorm();

    const Scalar det = G.determinant();
    const Scalar tol = std::numeric_limits<Scalar>::epsilon() * G(0, 0) * G(1, 1);

    if (std::abs(det) > tol) {
        // Non-degenerate triangle: direct 2x2 solve.
        const Eigen::Matrix<Scalar, 2, 1> b(ep.dot(e1), ep.dot(e2));
        const Eigen::Matrix<Scalar, 2, 1> uv = G.inverse() * b;

        Eigen::Matrix<Scalar, 3, 1> bary;
        bary(0) = Scalar(1) - uv(0) - uv(1);
        bary(1) = uv(0);
        bary(2) = uv(1);
        return bary;
    }

    // Slow path for degenerate triangles: constrained least-squares with partition-of-unity.
    //   [ v0  v1  v2 ]       [ p ]
    //   [  1   1   1 ] * b = [ 1 ]
    Eigen::Matrix<Scalar, Rows, 3, 0, MaxRows, 3> M(dim + 1, 3);
    M.row(dim).setOnes();
    for (Eigen::Index d = 0; d < dim; ++d) {
        M(d, 0) = v0[d];
        M(d, 1) = v1[d];
        M(d, 2) = v2[d];
    }

    Eigen::Matrix<Scalar, Rows, 1, 0, MaxRows, 1> rhs(dim + 1);
    for (Eigen::Index d = 0; d < dim; ++d) {
        rhs(d) = p[d];
    }
    rhs(dim) = Scalar(1);

    return M.colPivHouseholderQr().solve(rhs);
}


} // namespace lagrange

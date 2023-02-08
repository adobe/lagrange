// Source: https://github.com/alicevision/geogram/blob/master/src/lib/geogram/basic/geometry_nd.h
// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright (c) 2012-2014, Bruno Levy
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
// * Neither the name of the ALICE Project-Team nor the names of its
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// If you modify this software, you should include a notice giving the
// name of the person performing the modification, the date of modification,
// and the reason for such modification.
//
// Contact: Bruno Levy
//
//    Bruno.Levy@inria.fr
//    http://www.loria.fr/~levy
//
//    ALICE Project
//    LORIA, INRIA Lorraine,
//    Campus Scientifique, BP 239
//    54506 VANDOEUVRE LES NANCY CEDEX
//    FRANCE
//
// This file has been modified by Adobe.
//
// All modifications are Copyright 2020 Adobe.
//
#pragma once

#include <lagrange/common.h>

#include <Eigen/Dense>

namespace lagrange {

///
/// Computes the point closest to a given point in a nd segment
///
/// @param[in]  point          the query point
/// @param[in]  V0             first extremity of the segment
/// @param[in]  V1             second extremity of the segment
/// @param[out] closest_point  the point closest to @p point in the segment [\p V0, @p V1]
/// @param[out] lambda0        barycentric coordinate of the closest point relative to @p V0
/// @param[out] lambda1        barycentric coordinate of the closest point relative to @p V1
///
/// @tparam     PointType      the class that represents the points.
///
/// @return     the squared distance between the point and the segment [\p V0, @p V1]
///
template <typename PointType>
auto point_segment_squared_distance(
    const Eigen::MatrixBase<PointType>& point,
    const Eigen::MatrixBase<PointType>& V0,
    const Eigen::MatrixBase<PointType>& V1,
    Eigen::PlainObjectBase<PointType>& closest_point,
    ScalarOf<PointType>& lambda0,
    ScalarOf<PointType>& lambda1) -> ScalarOf<PointType>
{
    using Scalar = ScalarOf<PointType>;

    auto l2 = (V0 - V1).squaredNorm();
    auto t = (point - V0).dot(V1 - V0);
    if (t <= Scalar(0) || l2 == Scalar(0)) {
        closest_point = V0;
        lambda0 = Scalar(1);
        lambda1 = Scalar(0);
        return (point - V0).squaredNorm();
    } else if (t > l2) {
        closest_point = V1;
        lambda0 = Scalar(0);
        lambda1 = Scalar(1);
        return (point - V1).squaredNorm();
    }
    lambda1 = t / l2;
    lambda0 = Scalar(1) - lambda1;
    closest_point = lambda0 * V0 + lambda1 * V1;
    return (point - closest_point).squaredNorm();
}

///
/// Computes the point closest to a given point in a nd segment
///
/// @param[in]  point      the query point
/// @param[in]  V0         first extremity of the segment
/// @param[in]  V1         second extremity of the segment
///
/// @tparam     PointType  the class that represents the points.
///
/// @return     the squared distance between the point and the segment [\p V0, @p V1]
///
template <typename PointType>
auto point_segment_squared_distance(
    const Eigen::MatrixBase<PointType>& point,
    const Eigen::MatrixBase<PointType>& V0,
    const Eigen::MatrixBase<PointType>& V1) -> ScalarOf<PointType>
{
    PointType closest_point;
    ScalarOf<PointType> lambda0;
    ScalarOf<PointType> lambda1;
    return point_segment_squared_distance(point, V0, V1, closest_point, lambda0, lambda1);
}

} // namespace lagrange

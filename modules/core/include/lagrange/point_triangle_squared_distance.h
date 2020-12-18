// Source: https://github.com/alicevision/geogram/blob/master/src/lib/geogram/basic/geometry_nd.h
// License: BSD-3
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
// This file has been modified by Adobe to use Eigen for linear algebra.
//
// All modifications are Copyright 2020 Adobe.
//
#pragma once

#include <lagrange/common.h>
#include <lagrange/point_segment_squared_distance.h>

#include <Eigen/Core>

namespace lagrange {

///
/// Computes the point closest to a given point in a nd triangle. See
/// http://www.geometrictools.com/LibMathematics/Distance/Distance.html
///
/// @param[in]  point          the query point
/// @param[in]  V0             first vertex of the triangle
/// @param[in]  V1             second vertex of the triangle
/// @param[in]  V2             third vertex of the triangle
/// @param[out] closest_point  the point closest to @p point in the triangle (\p V0, @p V1, @p V2)
/// @param[out] lambda0        barycentric coordinate of the closest point relative to @p V0
/// @param[out] lambda1        barycentric coordinate of the closest point relative to @p V1
/// @param[out] lambda2        barycentric coordinate of the closest point relative to @p V2
///
/// @tparam     PointType      the class that represents the points.
///
/// @return     the squared distance between the point and the triangle (\p V0, @p V1, @p V2)
///
template <typename PointType>
auto point_triangle_squared_distance(
    const Eigen::MatrixBase<PointType>& point,
    const Eigen::MatrixBase<PointType>& V0,
    const Eigen::MatrixBase<PointType>& V1,
    const Eigen::MatrixBase<PointType>& V2,
    Eigen::PlainObjectBase<PointType>& closest_point,
    ScalarOf<PointType>& lambda0,
    ScalarOf<PointType>& lambda1,
    ScalarOf<PointType>& lambda2) -> ScalarOf<PointType>
{
    using Scalar = ScalarOf<PointType>;

    Eigen::Vector3d diff = V0.template cast<double>() - point.template cast<double>();
    Eigen::Vector3d edge0 = V1.template cast<double>() - V0.template cast<double>();
    Eigen::Vector3d edge1 = V2.template cast<double>() - V0.template cast<double>();
    double a00 = edge0.squaredNorm();
    double a01 = edge0.dot(edge1);
    double a11 = edge1.squaredNorm();
    double b0 = diff.dot(edge0);
    double b1 = diff.dot(edge1);
    double c = diff.squaredNorm();
    double det = std::fabs(a00 * a11 - a01 * a01);
    double s = a01 * b1 - a11 * b0;
    double t = a01 * b0 - a00 * b1;
    double sqr_distance;

    // If the triangle is degenerate
    if (det < 1e-30) {
        Scalar cur_l1, cur_l2;
        PointType cur_closest;
        Scalar result;
        Scalar cur_dist =
            point_segment_squared_distance(point, V0, V1, cur_closest, cur_l1, cur_l2);
        result = cur_dist;
        closest_point = cur_closest;
        lambda0 = cur_l1;
        lambda1 = cur_l2;
        lambda2 = 0;
        cur_dist = point_segment_squared_distance(point, V0, V2, cur_closest, cur_l1, cur_l2);
        if (cur_dist < result) {
            result = cur_dist;
            closest_point = cur_closest;
            lambda0 = cur_l1;
            lambda2 = cur_l2;
            lambda1 = Scalar(0);
        }
        cur_dist = point_segment_squared_distance(point, V1, V2, cur_closest, cur_l1, cur_l2);
        if (cur_dist < result) {
            result = cur_dist;
            closest_point = cur_closest;
            lambda1 = cur_l1;
            lambda2 = cur_l2;
            lambda0 = Scalar(0);
        }
        return result;
    }

    if (s + t <= det) {
        if (s < 0) {
            if (t < 0) { // region 4
                if (b0 < 0) {
                    t = 0;
                    if (-b0 >= a00) {
                        s = 1;
                        sqr_distance = a00 + 2 * b0 + c;
                    } else {
                        s = -b0 / a00;
                        sqr_distance = b0 * s + c;
                    }
                } else {
                    s = 0;
                    if (b1 >= 0) {
                        t = 0;
                        sqr_distance = c;
                    } else if (-b1 >= a11) {
                        t = 1;
                        sqr_distance = a11 + 2 * b1 + c;
                    } else {
                        t = -b1 / a11;
                        sqr_distance = b1 * t + c;
                    }
                }
            } else { // region 3
                s = 0;
                if (b1 >= 0) {
                    t = 0;
                    sqr_distance = c;
                } else if (-b1 >= a11) {
                    t = 1;
                    sqr_distance = a11 + 2 * b1 + c;
                } else {
                    t = -b1 / a11;
                    sqr_distance = b1 * t + c;
                }
            }
        } else if (t < 0) { // region 5
            t = 0;
            if (b0 >= 0) {
                s = 0;
                sqr_distance = c;
            } else if (-b0 >= a00) {
                s = 1;
                sqr_distance = a00 + 2 * b0 + c;
            } else {
                s = -b0 / a00;
                sqr_distance = b0 * s + c;
            }
        } else { // region 0
            // minimum at interior point
            double inv_det = double(1) / det;
            s *= inv_det;
            t *= inv_det;
            sqr_distance = s * (a00 * s + a01 * t + 2 * b0) + t * (a01 * s + a11 * t + 2 * b1) + c;
        }
    } else {
        double tmp0, tmp1, numer, denom;

        if (s < 0) { // region 2
            tmp0 = a01 + b0;
            tmp1 = a11 + b1;
            if (tmp1 > tmp0) {
                numer = tmp1 - tmp0;
                denom = a00 - 2 * a01 + a11;
                if (numer >= denom) {
                    s = 1;
                    t = 0;
                    sqr_distance = a00 + 2 * b0 + c;
                } else {
                    s = numer / denom;
                    t = 1 - s;
                    sqr_distance =
                        s * (a00 * s + a01 * t + 2 * b0) + t * (a01 * s + a11 * t + 2 * b1) + c;
                }
            } else {
                s = 0;
                if (tmp1 <= 0) {
                    t = 1;
                    sqr_distance = a11 + 2 * b1 + c;
                } else if (b1 >= 0) {
                    t = 0;
                    sqr_distance = c;
                } else {
                    t = -b1 / a11;
                    sqr_distance = b1 * t + c;
                }
            }
        } else if (t < 0) { // region 6
            tmp0 = a01 + b1;
            tmp1 = a00 + b0;
            if (tmp1 > tmp0) {
                numer = tmp1 - tmp0;
                denom = a00 - 2 * a01 + a11;
                if (numer >= denom) {
                    t = 1;
                    s = 0;
                    sqr_distance = a11 + 2 * b1 + c;
                } else {
                    t = numer / denom;
                    s = 1 - t;
                    sqr_distance =
                        s * (a00 * s + a01 * t + 2 * b0) + t * (a01 * s + a11 * t + 2 * b1) + c;
                }
            } else {
                t = 0;
                if (tmp1 <= 0) {
                    s = 1;
                    sqr_distance = a00 + 2 * b0 + c;
                } else if (b0 >= 0) {
                    s = 0;
                    sqr_distance = c;
                } else {
                    s = -b0 / a00;
                    sqr_distance = b0 * s + c;
                }
            }
        } else { // region 1
            numer = a11 + b1 - a01 - b0;
            if (numer <= 0) {
                s = 0;
                t = 1;
                sqr_distance = a11 + 2 * b1 + c;
            } else {
                denom = a00 - 2 * a01 + a11;
                if (numer >= denom) {
                    s = 1;
                    t = 0;
                    sqr_distance = a00 + 2 * b0 + c;
                } else {
                    s = numer / denom;
                    t = 1 - s;
                    sqr_distance =
                        s * (a00 * s + a01 * t + 2 * b0) + t * (a01 * s + a11 * t + 2 * b1) + c;
                }
            }
        }
    }

    // Account for numerical round-off error.
    if (sqr_distance < 0) {
        sqr_distance = 0;
    }

    closest_point = (V0.template cast<double>() + s * edge0 + t * edge1).template cast<Scalar>();
    lambda0 = Scalar(1 - s - t);
    lambda1 = Scalar(s);
    lambda2 = Scalar(t);
    return Scalar(sqr_distance);
}

///
/// @brief      Computes the squared distance between a point and a nd triangle.
/// @details    See http://www.geometrictools.com/LibMathematics/Distance/Distance.html
///
/// @param[in]  point      the query point
/// @param[in]  V0         first vertex of the triangle
/// @param[in]  V1         second vertex of the triangle
/// @param[in]  V2         third vertex of the triangle
///
/// @tparam     PointType  the class that represents the points.
///
/// @return     the squared distance between the point and the triangle (\p V0, @p V1, @p V2)
///
template <typename PointType>
auto point_triangle_squared_distance(
    const Eigen::MatrixBase<PointType>& point,
    const Eigen::MatrixBase<PointType>& V0,
    const Eigen::MatrixBase<PointType>& V1,
    const Eigen::MatrixBase<PointType>& V2) -> ScalarOf<PointType>
{
    PointType closest_point;
    ScalarOf<PointType> lambda1, lambda2, lambda3;
    return point_triangle_squared_distance(
        point,
        V0,
        V1,
        V2,
        closest_point,
        lambda1,
        lambda2,
        lambda3);
}

} // namespace lagrange

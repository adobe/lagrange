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

#include <lagrange/common.h>

namespace lagrange {

template <typename PointType>
auto compute_barycentric_coordinates(
    const Eigen::MatrixBase<PointType>& v0,
    const Eigen::MatrixBase<PointType>& v1,
    const Eigen::MatrixBase<PointType>& v2,
    const Eigen::MatrixBase<PointType>& p) -> Eigen::Matrix<typename PointType::Scalar, 3, 1>
{
    using Scalar = typename PointType::Scalar;
    const auto dim = p.size();
    Eigen::Matrix<Scalar, 3, 3> M;
    M.setZero();
    M.block(0, 0, dim, 1) = v0;
    M.block(0, 1, dim, 1) = v1;
    M.block(0, 2, dim, 1) = v2;
    Eigen::Matrix<Scalar, 3, 1> rhs;
    rhs.setZero();
    rhs.segment(0, dim) = p;
    return (M.inverse() * rhs).eval();
}


} // namespace lagrange

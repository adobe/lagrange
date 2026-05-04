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

#include <cmath>
#include <limits>

namespace lagrange::polyddg {

///
/// N-rosy encode: multiply the tangent-plane angle by n while preserving magnitude.
/// Given a 2D tangent vector (r cos θ, r sin θ), produce (r cos nθ, r sin nθ).
///
template <typename Scalar>
Eigen::Matrix<Scalar, 2, 1> nrosy_encode(const Eigen::Matrix<Scalar, 2, 1>& v, int n)
{
    Scalar r = v.norm();
    if (r < std::numeric_limits<Scalar>::epsilon()) return v;

    Scalar c = v(0) / r, s = v(1) / r;
    Scalar re = Scalar(1), im = Scalar(0);
    for (int k = 0; k < n; ++k) {
        Scalar new_re = re * c - im * s;
        Scalar new_im = re * s + im * c;
        re = new_re;
        im = new_im;
    }
    return {r * re, r * im};
}

///
/// N-rosy decode: divide the tangent-plane angle by n while preserving magnitude.
/// Given a 2D tangent vector (r cos φ, r sin φ), produce (r cos(φ/n), r sin(φ/n)).
///
template <typename Scalar>
Eigen::Matrix<Scalar, 2, 1> nrosy_decode(const Eigen::Matrix<Scalar, 2, 1>& v, int n)
{
    Scalar r = v.norm();
    if (r < std::numeric_limits<Scalar>::epsilon()) return v;

    Scalar phi = std::atan2(v(1), v(0));
    Scalar theta = phi / static_cast<Scalar>(n);
    return {r * std::cos(theta), r * std::sin(theta)};
}

} // namespace lagrange::polyddg

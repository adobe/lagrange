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

#include <cmath> // for std::exp

///
/// Utility functions that don't belong anywhere else.
///
/// Consider refactoring this file if it becomes too big, or categories emerge.
///

namespace lagrange {

/// Convert radians to degrees.
/// Use either as to_degrees(x) or x * to_degrees<double>();
template <typename Scalar>
Scalar to_degrees(Scalar rad = 1)
{
    return Scalar(180) * rad / Scalar(M_PI);
}

/// Convert degrees to radians.
/// Use either as to_degrees(x) or x * to_degrees<double>();
template <typename Scalar>
Scalar to_radians(Scalar deg = 1)
{
    return Scalar(M_PI) * deg / Scalar(180);
}

/// Get the sign of the value
/// Returns either -1, 0, or 1
template <typename T>
int sign(T val)
{
    return (T(0) < val) - (val < T(0));
}

/// Simple evaluation of Gaussian function
template <typename Scalar>
Scalar gaussian(Scalar x, Scalar sigma, Scalar center = 0)
{
    Scalar x2 = (x - center) * (x - center);
    Scalar exponent = x2 / (2 * sigma * sigma);
    return std::exp(-exponent);
}

} // namespace lagrange

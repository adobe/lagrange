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

#include <lagrange/utils/assert.h>

#include <Eigen/Dense>

#include <type_traits>

namespace lagrange::internal {

///
/// Linearly interpolate row `row_to` from rows `row_from_1` and `row_from_2` of an attribute
/// matrix: `data.row(row_to) = (1 - t) * data.row(row_from_1) + t * data.row(row_from_2)`.
/// Integer-valued attributes are interpolated in floating point and rounded.
///
/// @param[in,out] data        Attribute matrix. Row `row_to` is overwritten with the result.
/// @param[in]     row_to      Destination row index.
/// @param[in]     row_from_1  Source row index for weight `(1 - t)`.
/// @param[in]     row_from_2  Source row index for weight `t`.
/// @param[in]     t           Interpolation parameter in [0, 1].
///
/// @tparam Derived  Eigen matrix expression type.
/// @tparam Scalar   Floating-point type used for interpolation weights.
/// @tparam Index    Integral row index type.
///
template <typename Derived, typename Scalar, typename Index>
void interpolate_attribute_row(
    Eigen::MatrixBase<Derived>& data,
    Index row_to,
    Index row_from_1,
    Index row_from_2,
    Scalar t)
{
    la_debug_assert(row_to < static_cast<Index>(data.rows()));
    la_debug_assert(row_from_1 < static_cast<Index>(data.rows()));
    la_debug_assert(row_from_2 < static_cast<Index>(data.rows()));
    using ValueType = typename Derived::Scalar;

    if constexpr (std::is_integral_v<ValueType>) {
        data.row(row_to) = (data.row(row_from_1).template cast<Scalar>() * (1 - t) +
                            data.row(row_from_2).template cast<Scalar>() * t)
                               .array()
                               .round()
                               .template cast<ValueType>()
                               .eval();
    } else {
        data.row(row_to) = data.row(row_from_1) * (1 - t) + data.row(row_from_2) * t;
    }
}

} // namespace lagrange::internal

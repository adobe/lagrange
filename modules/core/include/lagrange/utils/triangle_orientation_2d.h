/*
 * Copyright 2023 Adobe. All rights reserved.
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

#include <lagrange/ExactPredicatesShewchuk.h>
#include <lagrange/utils/span.h>

namespace lagrange {

enum class Orientation : short { Positive = 1, Zero = 0, Negative = -1 };

/**
 * Compute orientation of a 2D triangle.
 *
 * @tparam Scalar  The scalar type.
 * @param a,b,c    2D coordinates of the triangle.
 *
 * @return  `Orientation::Positive` if positively oriented.
 *          `Orientation::Negative`  if negatively oriented.
 *          `Orientation::Zero` if the triangle is degenerate.
 */
template <typename Scalar>
Orientation
triangle_orientation(span<const Scalar, 2> a, span<const Scalar, 2> b, span<const Scalar, 2> c)
{
    const ExactPredicatesShewchuk predicates;
    if constexpr (std::is_same_v<Scalar, double>) {
        return static_cast<Orientation>(predicates.orient2D(
            const_cast<double*>(a.data()),
            const_cast<double*>(b.data()),
            const_cast<double*>(c.data())));
    } else {
        double data[6]{
            static_cast<double>(a[0]),
            static_cast<double>(a[1]),
            static_cast<double>(b[0]),
            static_cast<double>(b[1]),
            static_cast<double>(c[0]),
            static_cast<double>(c[1])};
        return static_cast<Orientation>(predicates.orient2D(data, data + 2, data + 4));
    }
}

} // namespace lagrange

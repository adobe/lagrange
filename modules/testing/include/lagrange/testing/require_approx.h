/*
 * Copyright 2024 Adobe. All rights reserved.
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

#include <Eigen/Dense>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace lagrange::testing {

template <typename DerivedA, typename DerivedB>
void require_approx(
    const Eigen::MatrixBase<DerivedA>& A,
    const Eigen::MatrixBase<DerivedB>& B,
    typename DerivedA::Scalar eps_rel,
    typename DerivedA::Scalar eps_abs)
{
    REQUIRE(A.rows() == B.rows());
    REQUIRE(A.cols() == B.cols());
    for (Eigen::Index i = 0; i < A.size(); ++i) {
        REQUIRE_THAT(
            A.derived().data()[i],
            Catch::Matchers::WithinRel(B.derived().data()[i], eps_rel) ||
                (Catch::Matchers::WithinAbs(0, eps_abs) &&
                 Catch::Matchers::WithinAbs(B.derived().data()[i], eps_abs)));
    }
}

} // namespace lagrange::testing

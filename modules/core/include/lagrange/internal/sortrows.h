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

#include <Eigen/Dense>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_sort.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
#include <numeric>

namespace lagrange::internal {

template <typename DerivedX, typename DerivedIX>
void sortrows(
    const Eigen::DenseBase<DerivedX>& X,
    const bool ascending,
    Eigen::PlainObjectBase<DerivedX>& Y,
    Eigen::PlainObjectBase<DerivedIX>& IX)
{
    using Index = typename DerivedIX::Scalar;

    // Resize output
    Y.resizeLike(X);
    IX.resize(X.rows(), 1);
    std::iota(IX.begin(), IX.end(), Index(0));

    auto run_sort = [&](auto cmp) {
        tbb::parallel_sort(IX.begin(), IX.end(), [&](Index i, Index j) {
            return std::lexicographical_compare(
                X.row(i).begin(),
                X.row(i).end(),
                X.row(j).begin(),
                X.row(j).end(),
                cmp);
        });
    };

    if (ascending) {
        run_sort(std::less<>());
    } else {
        run_sort(std::greater<>());
    }

    for (Eigen::Index i = 0; i < X.rows(); ++i) {
        Y.row(i) = X.row(IX(i));
    }
}

template <typename DerivedX>
void sortrows(
    const Eigen::DenseBase<DerivedX>& X,
    const bool ascending,
    Eigen::PlainObjectBase<DerivedX>& Y)
{
    Eigen::Matrix<int, DerivedX::RowsAtCompileTime, 1> I;
    return lagrange::internal::sortrows(X, ascending, Y, I);
}

} // namespace lagrange::internal

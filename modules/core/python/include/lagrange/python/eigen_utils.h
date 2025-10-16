/*
 * Copyright 2025 Adobe. All rights reserved.
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
#include <lagrange/Logger.h>
#include <lagrange/python/binding.h>

#include <Eigen/Core>

namespace lagrange::python {
namespace nb = nanobind;

template <typename Scalar, int Dim>
using Point = Eigen::Matrix<Scalar, 1, Dim>;

template <typename Scalar, int Dim>
using NBPoint = nb::ndarray<Scalar, nb::shape<Dim>, nb::c_contig, nb::device::cpu>;

template <typename Scalar, int Dim>
using GenericPoint = std::variant<nb::list, NBPoint<Scalar, Dim>>;


///
/// Convert a GenericPoint to an Eigen point.
///
/// @param[in]  p  The input point (either a python list or a NumPy array).
/// @return     The converted Eigen point.
///
template <typename Scalar, int Dim>
Point<Scalar, Dim> to_eigen_point(const GenericPoint<Scalar, Dim>& p)
{
    Point<Scalar, Dim> q;
    if (std::holds_alternative<nb::list>(p)) {
        auto lst = std::get<nb::list>(p);
        if (lst.size() != Dim) {
            throw std::runtime_error(fmt::format("Point list must have exactly {} elements.", Dim));
        }
        for (int i = 0; i < Dim; ++i) {
            q(i) = nb::cast<Scalar>(lst[i]);
        }
    } else {
        auto arr = std::get<NBPoint<Scalar, Dim>>(p);
        for (int i = 0; i < Dim; ++i) {
            q(i) = arr(i);
        }
    }
    return q;
}

} // namespace lagrange::python

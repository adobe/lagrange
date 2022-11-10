// Source: https://github.com/libigl/libigl/blob/main/include/igl/doublearea.cpp
// SPDX-License-Identifier: MPL-2.0
//
// This file is part of libigl, a simple c++ geometry processing library.
//
// Copyright (C) 2013 Alec Jacobson <alecjacobson@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public License
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at http://mozilla.org/MPL/2.0/.
//
// This file has been modified by Adobe.
//
// All modifications are Copyright 2022 Adobe.
#pragma once

#include <lagrange/utils/assert.h>

#include <Eigen/Dense>

namespace lagrange::internal {

template <typename DerivedV, typename DerivedF, typename DeriveddblA>
void doublearea(
    const Eigen::MatrixBase<DerivedV>& V,
    const Eigen::MatrixBase<DerivedF>& F,
    Eigen::PlainObjectBase<DeriveddblA>& dblA)
{
    const auto dim = V.cols();
    const auto m = F.rows();

    // Only support triangles
    la_runtime_assert(F.cols() == 3);

    // Only support dim 2 or 3
    la_runtime_assert(dim == 2 || dim == 3);

    // Compute edge lengths
    Eigen::Matrix<typename DerivedV::Scalar, Eigen::Dynamic, 3> l;

    // Projected area helper
    const auto& proj_doublearea = [&V, &F](Eigen::Index x, Eigen::Index y, Eigen::Index f) ->
        typename DerivedV::Scalar {
            auto rx = V(F(f, 0), x) - V(F(f, 2), x);
            auto sx = V(F(f, 1), x) - V(F(f, 2), x);
            auto ry = V(F(f, 0), y) - V(F(f, 2), y);
            auto sy = V(F(f, 1), y) - V(F(f, 2), y);
            return rx * sy - ry * sx;
        };

    switch (dim) {
    case 3: {
        dblA = DeriveddblA::Zero(m, 1);
        for (Eigen::Index f = 0; f < m; f++) {
            for (int d = 0; d < 3; d++) {
                const auto dblAd = proj_doublearea(d, (d + 1) % 3, f);
                dblA(f) += dblAd * dblAd;
            }
        }
        dblA = dblA.array().sqrt().eval();
        break;
    }
    case 2: {
        dblA.resize(m, 1);
        for (Eigen::Index f = 0; f < m; f++) {
            dblA(f) = proj_doublearea(0, 1, f);
        }
        break;
    }
    default: break;
    }
}

} // namespace lagrange::internal

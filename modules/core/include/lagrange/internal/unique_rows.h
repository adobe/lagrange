// Source: https://github.com/libigl/libigl/blob/main/include/igl/unique_rows.cpp
// SPDX-License-Identifier: MPL-2.0
//
// This file is part of libigl, a simple c++ geometry processing library.
//
// Copyright (C) 2017 Alec Jacobson <alecjacobson@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public License
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at http://mozilla.org/MPL/2.0/.
//
// This file has been modified by Adobe.
//
// All modifications are Copyright 2022 Adobe.
#pragma once

#include <lagrange/internal/sortrows.h>

namespace lagrange::internal {

template <typename DerivedA, typename DerivedC, typename DerivedIA, typename DerivedIC>
void unique_rows(
    const Eigen::DenseBase<DerivedA>& A,
    Eigen::PlainObjectBase<DerivedC>& C,
    Eigen::PlainObjectBase<DerivedIA>& IA,
    Eigen::PlainObjectBase<DerivedIC>& IC)
{
    Eigen::VectorXi IM;
    DerivedA sortA;
    lagrange::internal::sortrows(A, true, sortA, IM);

    const int num_rows = static_cast<int>(sortA.rows());
    const int num_cols = static_cast<int>(sortA.cols());
    std::vector<int> vIA(num_rows);
    for (int i = 0; i < num_rows; i++) {
        vIA[i] = i;
    }

    auto index_equal = [&](const size_t i, const size_t j) {
        for (size_t c = 0; c < num_cols; c++) {
            if (sortA(i, c) != sortA(j, c)) return false;
        }
        return true;
    };
    vIA.erase(std::unique(vIA.begin(), vIA.end(), index_equal), vIA.end());

    IC.resize(A.rows(), 1);
    {
        int j = 0;
        for (int i = 0; i < num_rows; i++) {
            if (sortA.row(vIA[j]) != sortA.row(i)) {
                j++;
            }
            IC(IM(i, 0), 0) = j;
        }
    }
    const int unique_rows = static_cast<int>(vIA.size());
    C.resize(unique_rows, A.cols());
    IA.resize(unique_rows, 1);
    // Reindex IA according to IM
    for (int i = 0; i < unique_rows; i++) {
        IA(i, 0) = IM(vIA[i], 0);
        C.row(i) << A.row(IA(i, 0));
    }
}

} // namespace lagrange::internal

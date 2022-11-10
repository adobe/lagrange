// Source: https://github.com/libigl/libigl/blob/main/include/igl/vertex_components.cpp
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

#include <Eigen/Dense>
#include <Eigen/Sparse>

#include <queue>
#include <vector>

namespace lagrange::internal {

template <typename DerivedA, typename DerivedC, typename Derivedcounts>
void vertex_components(
    const Eigen::SparseCompressedBase<DerivedA>& A,
    Eigen::PlainObjectBase<DerivedC>& C,
    Eigen::PlainObjectBase<Derivedcounts>& counts)
{
    assert(A.rows() == A.cols() && "A should be square.");
    const size_t n = A.rows();
    Eigen::Array<bool, Eigen::Dynamic, 1> seen = Eigen::Array<bool, Eigen::Dynamic, 1>::Zero(n, 1);
    C.resize(n, 1);
    typename DerivedC::Scalar id = 0;
    std::vector<typename Derivedcounts::Scalar> vcounts;
    // breadth first search
    for (int k = 0; k < A.outerSize(); ++k) {
        if (seen(k)) {
            continue;
        }
        std::queue<int> Q;
        Q.push(k);
        vcounts.push_back(0);
        while (!Q.empty()) {
            const int f = Q.front();
            Q.pop();
            if (seen(f)) {
                continue;
            }
            seen(f) = true;
            C(f, 0) = id;
            vcounts[id]++;
            // Iterate over inside
            for (typename DerivedA::InnerIterator it(A, f); it; ++it) {
                const int g = it.index();
                if (!seen(g) && it.value()) {
                    Q.push(g);
                }
            }
        }
        id++;
    }
    assert((size_t)id == vcounts.size());
    const size_t ncc = vcounts.size();
    assert((size_t)C.maxCoeff() + 1 == ncc);
    counts.resize(ncc, 1);
    for (size_t i = 0; i < ncc; i++) {
        counts(i) = vcounts[i];
    }
}

template <typename DerivedA, typename DerivedC>
void vertex_components(
    const Eigen::SparseCompressedBase<DerivedA>& A,
    Eigen::PlainObjectBase<DerivedC>& C)
{
    Eigen::VectorXi counts;
    return vertex_components(A, C, counts);
}

} // namespace lagrange::internal

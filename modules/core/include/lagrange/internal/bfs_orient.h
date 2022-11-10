// Source: https://github.com/libigl/libigl/blob/main/include/igl/bfs_orient.cpp
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

#include <lagrange/internal/orientable_patches.h>

namespace lagrange::internal {

template <typename DerivedF, typename DerivedFF, typename DerivedC>
void bfs_orient(
    const Eigen::MatrixBase<DerivedF>& F,
    Eigen::PlainObjectBase<DerivedFF>& FF,
    Eigen::PlainObjectBase<DerivedC>& C)
{
    Eigen::SparseMatrix<typename DerivedF::Scalar> A;
    lagrange::internal::orientable_patches(F, C, A);

    // number of faces
    const int m = F.rows();
    // number of patches
    const int num_cc = C.maxCoeff() + 1;
    Eigen::VectorXi seen = Eigen::VectorXi::Zero(m);

    // Edge sets
    const int ES[3][2] = {{1, 2}, {2, 0}, {0, 1}};

    if (((void*)&FF) != ((void*)&F)) {
        FF = F;
    }
    // loop over patches
    tbb::parallel_for(0, num_cc, [&](int c) {
        std::queue<typename DerivedF::Scalar> Q;
        // find first member of patch c
        for (int f = 0; f < FF.rows(); f++) {
            if (C(f) == c) {
                Q.push(f);
                break;
            }
        }
        assert(!Q.empty());
        while (!Q.empty()) {
            const typename DerivedF::Scalar f = Q.front();
            Q.pop();
            if (seen(f) > 0) {
                continue;
            }
            seen(f)++;
            // loop over neighbors of f
            for (typename Eigen::SparseMatrix<typename DerivedF::Scalar>::InnerIterator it(A, f);
                 it;
                 ++it) {
                // might be some lingering zeros, and skip self-adjacency
                if (it.value() != 0 && it.row() != f) {
                    const int n = it.row();
                    assert(n != f);
                    // loop over edges of f
                    for (int efi = 0; efi < 3; efi++) {
                        // efi'th edge of face f
                        Eigen::Vector2i ef(FF(f, ES[efi][0]), FF(f, ES[efi][1]));
                        // loop over edges of n
                        for (int eni = 0; eni < 3; eni++) {
                            // eni'th edge of face n
                            Eigen::Vector2i en(FF(n, ES[eni][0]), FF(n, ES[eni][1]));
                            // Match (half-edges go same direction)
                            if (ef(0) == en(0) && ef(1) == en(1)) {
                                // flip face n
                                FF.row(n) = FF.row(n).reverse().eval();
                            }
                        }
                    }
                    // add neighbor to queue
                    Q.push(n);
                }
            }
        }
    });

    // make sure flip is OK if &FF = &F
}

} // namespace lagrange::internal

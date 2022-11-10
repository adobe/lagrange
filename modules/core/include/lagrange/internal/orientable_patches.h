// Source: https://github.com/libigl/libigl/blob/main/include/igl/orientable_patches.cpp
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

#include <lagrange/internal/unique_rows.h>
#include <lagrange/internal/vertex_components.h>

namespace lagrange::internal {

template <typename DerivedF, typename DerivedC, typename AScalar>
void orientable_patches(
    const Eigen::MatrixBase<DerivedF>& F,
    Eigen::PlainObjectBase<DerivedC>& C,
    Eigen::SparseMatrix<AScalar>& A)
{
    // simplex size
    assert(F.cols() == 3);

    // List of all "half"-edges: 3*#F by 2
    Eigen::Matrix<typename DerivedF::Scalar, Eigen::Dynamic, 2> allE, uE;
    allE.resize(F.rows() * 3, 2);
    Eigen::Matrix<typename DerivedF::Scalar, Eigen::Dynamic, 2> IX;
    Eigen::VectorXi IA, IC;
    allE.block(0 * F.rows(), 0, F.rows(), 1) = F.col(1);
    allE.block(0 * F.rows(), 1, F.rows(), 1) = F.col(2);
    allE.block(1 * F.rows(), 0, F.rows(), 1) = F.col(2);
    allE.block(1 * F.rows(), 1, F.rows(), 1) = F.col(0);
    allE.block(2 * F.rows(), 0, F.rows(), 1) = F.col(0);
    allE.block(2 * F.rows(), 1, F.rows(), 1) = F.col(1);
    // Sort each row
    for (auto row : allE.rowwise()) {
        std::sort(row.begin(), row.end());
    }
    // IC(i) tells us where to find sortallE(i,:) in uE:
    //  so that sortallE(i,:) = uE(IC(i),:)
    lagrange::internal::unique_rows(allE, uE, IA, IC);
    // uE2FT(e,f) = 1 means face f is adjacent to unique edge e
    std::vector<Eigen::Triplet<AScalar>> uE2FTijv(IC.rows());
    for (int e = 0; e < IC.rows(); e++) {
        uE2FTijv[e] = Eigen::Triplet<AScalar>(e % F.rows(), IC(e), 1);
    }
    Eigen::SparseMatrix<AScalar> uE2FT(F.rows(), uE.rows());
    uE2FT.setFromTriplets(uE2FTijv.begin(), uE2FTijv.end());
    // kill non-manifold edges
    for (int j = 0; j < (int)uE2FT.outerSize(); j++) {
        int degree = 0;
        for (typename Eigen::SparseMatrix<AScalar>::InnerIterator it(uE2FT, j); it; ++it) {
            degree++;
        }
        // Iterate over inside
        if (degree > 2) {
            for (typename Eigen::SparseMatrix<AScalar>::InnerIterator it(uE2FT, j); it; ++it) {
                uE2FT.coeffRef(it.row(), it.col()) = 0;
            }
        }
    }
    // Face-face Adjacency matrix
    Eigen::SparseMatrix<AScalar> uE2F;
    uE2F = uE2FT.transpose().eval();
    A = uE2FT * uE2F;
    // All ones
    for (int j = 0; j < A.outerSize(); j++) {
        // Iterate over inside
        for (typename Eigen::SparseMatrix<AScalar>::InnerIterator it(A, j); it; ++it) {
            if (it.value() > 1) {
                A.coeffRef(it.row(), it.col()) = 1;
            }
        }
    }
    //% Connected components are patches
    //%C = vertex_components(A); % alternative to graphconncomp from matlab_bgl
    //[~,C] = graphconncomp(A);
    // graph connected components
    lagrange::internal::vertex_components(A, C);
}

template <typename DerivedF, typename DerivedC>
void orientable_patches(const Eigen::MatrixBase<DerivedF>& F, Eigen::PlainObjectBase<DerivedC>& C)
{
    Eigen::SparseMatrix<typename DerivedF::Scalar> A;
    return lagrange::internal::orientable_patches(F, C, A);
}

} // namespace lagrange::internal

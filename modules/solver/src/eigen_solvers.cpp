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
#include <lagrange/solver/eigen_solvers.h>

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/solver/api.h>
#include <lagrange/utils/Error.h>

#include <Eigen/Core>
#include <Eigen/Sparse>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <Spectra/MatOp/SparseCholesky.h>
#include <Spectra/MatOp/SparseSymShiftSolve.h>
#include <Spectra/SymEigsShiftSolver.h>
#include <Spectra/SymEigsSolver.h>
#include <Spectra/SymGEigsShiftSolver.h>
#include <Spectra/SymGEigsSolver.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
#include <utility>

namespace lagrange::solver {

namespace {

Status convert_info(Spectra::CompInfo info)
{
    switch (info) {
    case Spectra::CompInfo::Successful: return Status::Successful;
    case Spectra::CompInfo::NotComputed: return Status::NotComputed;
    case Spectra::CompInfo::NotConverging: return Status::NotConverging;
    case Spectra::CompInfo::NumericalIssue: return Status::NumericalIssue;
    default: throw Error("Unknown Spectra::CompInfo value.");
    }
}

} // namespace

template <typename Scalar>
EigenResult<Scalar> selfadjoint_eigen_largest(const Eigen::SparseMatrix<Scalar>& A, size_t k)
{
    using OpType = Spectra::SparseSymMatProd<Scalar>;
    OpType op_A(A);

    Spectra::SymEigsSolver<OpType> eigs(
        op_A,
        k,
        std::min<size_t>(k * 2 + 1, static_cast<size_t>(A.cols())));
    eigs.init();

    int nconv = eigs.compute(Spectra::SortRule::LargestMagn);
    auto info = eigs.info();

    EigenResult<Scalar> result;
    result.num_converged = nconv;
    result.info = convert_info(info);

    if (info == Spectra::CompInfo::Successful) {
        result.eigenvalues = eigs.eigenvalues();
        result.eigenvectors = eigs.eigenvectors();
    }

    return result;
}

template <typename Scalar>
EigenResult<Scalar> selfadjoint_eigen_smallest(const Eigen::SparseMatrix<Scalar>& A, size_t k)
{
    using OpType = Spectra::SparseSymShiftSolve<Scalar>;
    OpType op_A(A);

    Spectra::SymEigsShiftSolver<OpType> eigs(
        op_A,
        k,
        std::min<size_t>(k * 2 + 1, static_cast<size_t>(A.cols())),
        0.0);
    eigs.init();

    int nconv = eigs.compute(Spectra::SortRule::LargestMagn);
    auto info = eigs.info();

    EigenResult<Scalar> result;
    result.num_converged = nconv;
    result.info = convert_info(info);

    if (info == Spectra::CompInfo::Successful) {
        result.eigenvalues = eigs.eigenvalues();
        result.eigenvectors = eigs.eigenvectors();
    }

    return result;
}

template <typename Scalar>
EigenResult<Scalar> generalized_selfadjoint_eigen_largest(
    const Eigen::SparseMatrix<Scalar>& A,
    const Eigen::SparseMatrix<Scalar>& M,
    size_t k)
{
    using OpType = Spectra::SparseSymMatProd<Scalar>;
    using MOpType = Spectra::SparseCholesky<Scalar>;
    OpType op_A(A);
    MOpType op_M(M);

    Spectra::SymGEigsSolver<OpType, MOpType, Spectra::GEigsMode::Cholesky> eigs(
        op_A,
        op_M,
        k,
        std::min<size_t>(k * 2 + 1, static_cast<size_t>(A.cols())));
    eigs.init();

    int nconv = eigs.compute(Spectra::SortRule::LargestMagn);
    auto info = eigs.info();

    EigenResult<Scalar> result;
    result.num_converged = nconv;
    result.info = convert_info(info);

    if (info == Spectra::CompInfo::Successful) {
        result.eigenvalues = eigs.eigenvalues();
        result.eigenvectors = eigs.eigenvectors();
    }

    return result;
}

///
/// Compute the k smallest magnitude eigenvalues and eigenvectors for a generalized eigenvalue
/// problem.
///
/// Solves: A * x = lambda * M * x
///
/// @tparam Scalar  The scalar type (e.g., float, double).
///
/// @param[in] A  A symmetric sparse matrix.
/// @param[in] M  A symmetric positive-definite sparse matrix.
/// @param[in] k  Number of eigenvalues/eigenvectors to compute.
///
/// @return       An EigenResult containing eigenvalues, eigenvectors, and computation status.
///
/// @note This function uses shift-invert mode with shift=0, which may fail if A is singular
///       or nearly singular. The function does not verify that A and M are symmetric or that
///       M is positive-definite.
///
template <typename Scalar>
EigenResult<Scalar> generalized_selfadjoint_eigen_smallest(
    const Eigen::SparseMatrix<Scalar>& A,
    const Eigen::SparseMatrix<Scalar>& M,
    size_t k)
{
    using OpType = Spectra::SymShiftInvert<Scalar, Eigen::Sparse, Eigen::Sparse>;
    using MOpType = Spectra::SparseSymMatProd<Scalar>;
    OpType op_A(A, M);
    MOpType op_M(M);

    Spectra::SymGEigsShiftSolver<OpType, MOpType, Spectra::GEigsMode::ShiftInvert> eigs(
        op_A,
        op_M,
        k,
        std::min<size_t>(k * 2 + 1, static_cast<size_t>(A.cols())),
        0.0);
    eigs.init();

    int nconv = eigs.compute(Spectra::SortRule::LargestMagn);
    auto info = eigs.info();

    EigenResult<Scalar> result;
    result.num_converged = nconv;
    result.info = convert_info(info);

    if (info == Spectra::CompInfo::Successful) {
        result.eigenvalues = eigs.eigenvalues();
        result.eigenvectors = eigs.eigenvectors();
    }

    return result;
}

#define LA_X_EigenSolver(_, Scalar)                                                    \
    template LA_SOLVER_API EigenResult<Scalar> selfadjoint_eigen_largest(              \
        const Eigen::SparseMatrix<Scalar>& A,                                          \
        size_t k);                                                                     \
    template LA_SOLVER_API EigenResult<Scalar> selfadjoint_eigen_smallest(             \
        const Eigen::SparseMatrix<Scalar>& A,                                          \
        size_t k);                                                                     \
    template LA_SOLVER_API EigenResult<Scalar> generalized_selfadjoint_eigen_largest(  \
        const Eigen::SparseMatrix<Scalar>& A,                                          \
        const Eigen::SparseMatrix<Scalar>& M,                                          \
        size_t k);                                                                     \
    template LA_SOLVER_API EigenResult<Scalar> generalized_selfadjoint_eigen_smallest( \
        const Eigen::SparseMatrix<Scalar>& A,                                          \
        const Eigen::SparseMatrix<Scalar>& M,                                          \
        size_t k);

LA_SURFACE_MESH_SCALAR_X(EigenSolver, 0)

} // namespace lagrange::solver

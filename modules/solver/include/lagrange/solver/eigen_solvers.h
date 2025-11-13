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

#include <Eigen/Core>
#include <Eigen/Sparse>

namespace lagrange::solver {

///
/// Solver status.
///
enum class Status { Successful, NotComputed, NotConverging, NumericalIssue };

///
/// Result structure for eigenvalue computations.
///
/// @tparam Scalar  The scalar type (e.g., float, double).
///
template <typename Scalar>
struct EigenResult
{
    /// Computed eigenvalues.
    Eigen::Matrix<Scalar, Eigen::Dynamic, 1> eigenvalues;

    /// Computed eigenvectors (column-wise).
    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> eigenvectors;

    /// Number of converged eigenvalues.
    size_t num_converged = 0;

    /// Computation status
    Status info = Status::NotComputed;

    /// Check if the computation was successful.
    bool is_successful() const { return info == Status::Successful; }
};

///
/// Compute the k largest magnitude eigenvalues and eigenvectors of a symmetric matrix.
///
/// @tparam Scalar  The scalar type (e.g., float, double).
///
/// @param[in] A  A symmetric sparse matrix.
/// @param[in] k  Number of eigenvalues/eigenvectors to compute.
///
/// @return       An EigenResult containing eigenvalues, eigenvectors, and computation status.
///
/// @note This function does not verify that A is symmetric. The caller must ensure this.
///
template <typename Scalar>
EigenResult<Scalar> selfadjoint_eigen_largest(const Eigen::SparseMatrix<Scalar>& A, size_t k);

///
/// Compute the k smallest magnitude eigenvalues and eigenvectors of a symmetric matrix.
///
/// @tparam Scalar  The scalar type (e.g., float, double).
///
/// @param[in] A  A symmetric sparse matrix.
/// @param[in] k  Number of eigenvalues/eigenvectors to compute.
///
/// @return       An EigenResult containing eigenvalues, eigenvectors, and computation status.
///
/// @note This function uses shift-invert mode with shift=0, which may fail if A is singular
///       or nearly singular. The function does not verify that A is symmetric.
///
template <typename Scalar>
EigenResult<Scalar> selfadjoint_eigen_smallest(const Eigen::SparseMatrix<Scalar>& A, size_t k);

///
/// Compute the k largest magnitude eigenvalues and eigenvectors for a generalized eigenvalue
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
/// @note This function does not verify that A and M are symmetric or that M is positive-definite.
///       The caller must ensure these properties hold.
///
template <typename Scalar>
EigenResult<Scalar> generalized_selfadjoint_eigen_largest(
    const Eigen::SparseMatrix<Scalar>& A,
    const Eigen::SparseMatrix<Scalar>& M,
    size_t k);

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
    size_t k);

} // namespace lagrange::solver

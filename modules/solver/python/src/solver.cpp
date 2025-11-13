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

#include <lagrange/Logger.h>
#include <lagrange/python/binding.h>
#include <lagrange/python/solver.h>
#include <lagrange/solver/eigen_solvers.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>

#include <lagrange/python/setup_mkl.h>

#include <Eigen/Sparse>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanobind/nanobind.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::python {

namespace nb = nanobind;

void populate_solver_module(nb::module_& m)
{
    setup_mkl();

    using Scalar = double;
    using namespace nanobind::literals;
    using SparseMatrix = Eigen::SparseMatrix<Scalar>;

    auto check_comp_info = [](typename solver::Status info) {
        switch (info) {
        case solver::Status::Successful: logger().info("Eigen decomposition successful."); break;
        case solver::Status::NotConverging:
            logger().error("Eigen decomposition did not converge.");
            break;
        case solver::Status::NotComputed:
            logger().error("Eigen decomposition was not computed.");
            break;
        case solver::Status::NumericalIssue:
            logger().error("Numerical issues encountered during eigen decomposition.");
            break;
        }
    };

    auto selfadjoint_eigen_largest = [&](const SparseMatrix& A, size_t k) {
        auto result = solver::selfadjoint_eigen_largest<Scalar>(A, k);
        logger().debug("Number of converged eigen values: {}", result.num_converged);
        check_comp_info(result.info);
        return nb::make_tuple(result.eigenvalues, result.eigenvectors);
    };

    auto selfadjoint_eigen_smallest = [&](const SparseMatrix& A, size_t k) {
        auto result = solver::selfadjoint_eigen_smallest<Scalar>(A, k);
        logger().debug("Number of converged eigen values: {}", result.num_converged);
        check_comp_info(result.info);
        return nb::make_tuple(result.eigenvalues, result.eigenvectors);
    };

    auto generalized_selfadjoint_eigen_largest =
        [&](const SparseMatrix& A, const SparseMatrix& M, size_t k) {
            auto result = solver::generalized_selfadjoint_eigen_largest<Scalar>(A, M, k);
            logger().debug("Number of converged eigen values: {}", result.num_converged);
            check_comp_info(result.info);
            return nb::make_tuple(result.eigenvalues, result.eigenvectors);
        };

    auto generalized_selfadjoint_eigen_smallest =
        [&](const SparseMatrix& A, const SparseMatrix& M, size_t k) {
            auto result = solver::generalized_selfadjoint_eigen_smallest<Scalar>(A, M, k);
            logger().debug("Number of converged eigen values: {}", result.num_converged);
            check_comp_info(result.info);
            return nb::make_tuple(result.eigenvalues, result.eigenvectors);
        };


    m.def(
        "eigsh",
        [&](const Eigen::SparseMatrix<Scalar>& A,
            size_t k,
            const std::optional<Eigen::SparseMatrix<Scalar>>& M,
            std::string_view which) {
            if (which == "LM") {
                if (M.has_value()) {
                    return generalized_selfadjoint_eigen_largest(A, M.value(), k);
                } else {
                    return selfadjoint_eigen_largest(A, k);
                }
            } else if (which == "SM") {
                if (M.has_value()) {
                    return generalized_selfadjoint_eigen_smallest(A, M.value(), k);
                } else {
                    return selfadjoint_eigen_smallest(A, k);
                }
            } else {
                logger().error("Which='{}' is not supported.", which);
                throw Error("Unsupported eigenvalue type requested.");
            }
        },
        "A"_a,
        "k"_a = 1,
        "M"_a = nb::none(),
        "which"_a = "LM",
        R"(Find k eigenvalues and eigenvectors of the symmetric square matrix A.

Solves ``A @ x[i] = w[i] * x[i]`` for k eigenvalues w[i] and eigenvectors x[i]
of a symmetric matrix A. Alternatively, for a generalized eigenvalue problem
when M is provided, solves ``A @ x[i] = w[i] * M @ x[i]``.

This function is designed to mimic the API of scipy.sparse.linalg.eigsh and uses
the Spectra library for sparse eigenvalue computation.

:param A: A symmetric square matrix with shape (n, n). Matrix A must be symmetric;
    this is not checked by the function.
:type A: sparse matrix
:param k: The number of eigenvalues and eigenvectors to compute. Must be 1 <= k < n.
    Default: 1.
:type k: int
:param M: A symmetric positive-definite matrix with the same shape as A for the
    generalized eigenvalue problem ``A @ x = w * M @ x``. If None (default),
    the standard eigenvalue problem is solved. Default: None.
:type M: sparse matrix or None
:param which: Which k eigenvectors and eigenvalues to find:
    'LM' for largest (in magnitude) eigenvalues, or
    'SM' for smallest (in magnitude) eigenvalues.
    Default: 'LM'.
:type which: str

:return: A tuple (w, v) where w is an array of k eigenvalues and v is an array of
    k eigenvectors with shape (n, k). The column v[:, i] is the eigenvector
    corresponding to the eigenvalue w[i].
:rtype: tuple[ndarray, ndarray]

.. note::
    This implementation uses the Spectra library and currently supports only 'LM' and
    'SM' options for the 'which' parameter. The eigenvalues are returned in descending
    order of magnitude for 'LM' and ascending order for 'SM'.

    For 'SM', this function uses shift-invert mode with shift=0, which may fail if
    the matrix A (or A - sigma*M for generalized problems) is singular or nearly singular.

.. seealso::
    :py:func:`scipy.sparse.linalg.eigsh` - SciPy's sparse symmetric eigenvalue solver
)");
}

} // namespace lagrange::python

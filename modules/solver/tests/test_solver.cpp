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
#include <lagrange/solver/DirectSolver.h>
#include <lagrange/solver/eigen_solvers.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("SolverLDLT", "[solver]")
{
    using Solver = lagrange::solver::SolverLDLT<Eigen::SparseMatrix<double>>;

    Eigen::MatrixXd A(3, 3);
    A << 4, -1, 2, -1, 6, 0, 2, 0, 5;
    Eigen::VectorXd b(3);
    b << 12, -25, 32;

    Eigen::SparseMatrix<double> A_sparse = A.sparseView();
    Solver solver(A_sparse);
    Eigen::VectorXd x = solver.solve(b);

    REQUIRE(solver.info() == Eigen::Success);
    REQUIRE((A * x).isApprox(b));
    REQUIRE((A_sparse * x).isApprox(b));
}

TEST_CASE("selfadjoint_eigen_largest", "[solver][eigen]")
{
    using Scalar = double;

    // Create a simple symmetric matrix with known eigenvalues
    // Matrix: [[2, 1], [1, 2]] has eigenvalues 3 and 1
    Eigen::SparseMatrix<Scalar> A(2, 2);
    std::vector<Eigen::Triplet<Scalar>> triplets;
    triplets.emplace_back(0, 0, 2.0);
    triplets.emplace_back(0, 1, 1.0);
    triplets.emplace_back(1, 0, 1.0);
    triplets.emplace_back(1, 1, 2.0);
    A.setFromTriplets(triplets.begin(), triplets.end());

    auto result = lagrange::solver::selfadjoint_eigen_largest<Scalar>(A, 1);

    REQUIRE(result.is_successful());
    REQUIRE(result.num_converged >= 1);
    REQUIRE(result.eigenvalues.size() >= 1);
    REQUIRE(result.eigenvectors.rows() == 2);

    // The largest eigenvalue should be 3
    REQUIRE_THAT(std::abs(result.eigenvalues(0)), Catch::Matchers::WithinAbs(3.0, 1e-6));
}

TEST_CASE("selfadjoint_eigen_smallest", "[solver][eigen]")
{
    using Scalar = double;

    // Create a simple symmetric matrix with known eigenvalues
    // Matrix: [[4, 1], [1, 4]] has eigenvalues 5 and 3
    Eigen::SparseMatrix<Scalar> A(2, 2);
    std::vector<Eigen::Triplet<Scalar>> triplets;
    triplets.emplace_back(0, 0, 4.0);
    triplets.emplace_back(0, 1, 1.0);
    triplets.emplace_back(1, 0, 1.0);
    triplets.emplace_back(1, 1, 4.0);
    A.setFromTriplets(triplets.begin(), triplets.end());

    auto result = lagrange::solver::selfadjoint_eigen_smallest<Scalar>(A, 1);

    REQUIRE(result.is_successful());
    REQUIRE(result.num_converged >= 1);
    REQUIRE(result.eigenvalues.size() >= 1);
    REQUIRE(result.eigenvectors.rows() == 2);

    // The smallest eigenvalue should be 3
    REQUIRE_THAT(std::abs(result.eigenvalues(0)), Catch::Matchers::WithinAbs(3.0, 1e-6));
}

TEST_CASE("generalized_selfadjoint_eigen_largest", "[solver][eigen]")
{
    using Scalar = double;

    // Create simple symmetric matrices
    // A: [[2, 0], [0, 3]]
    // M: [[1, 0], [0, 1]] (identity)
    // Eigenvalues should be 3 and 2
    Eigen::SparseMatrix<Scalar> A(2, 2);
    std::vector<Eigen::Triplet<Scalar>> triplets_A;
    triplets_A.emplace_back(0, 0, 2.0);
    triplets_A.emplace_back(1, 1, 3.0);
    A.setFromTriplets(triplets_A.begin(), triplets_A.end());

    Eigen::SparseMatrix<Scalar> M(2, 2);
    std::vector<Eigen::Triplet<Scalar>> triplets_M;
    triplets_M.emplace_back(0, 0, 1.0);
    triplets_M.emplace_back(1, 1, 1.0);
    M.setFromTriplets(triplets_M.begin(), triplets_M.end());

    auto result = lagrange::solver::generalized_selfadjoint_eigen_largest<Scalar>(A, M, 1);

    REQUIRE(result.is_successful());
    REQUIRE(result.num_converged >= 1);
    REQUIRE(result.eigenvalues.size() >= 1);
    REQUIRE(result.eigenvectors.rows() == 2);

    // The largest eigenvalue should be 3
    REQUIRE_THAT(std::abs(result.eigenvalues(0)), Catch::Matchers::WithinAbs(3.0, 1e-6));
}

TEST_CASE("generalized_selfadjoint_eigen_smallest", "[solver][eigen]")
{
    using Scalar = double;

    // Create simple symmetric matrices
    // A: [[3, 0], [0, 5]]
    // M: [[1, 0], [0, 1]] (identity)
    // Eigenvalues should be 5 and 3
    Eigen::SparseMatrix<Scalar> A(2, 2);
    std::vector<Eigen::Triplet<Scalar>> triplets_A;
    triplets_A.emplace_back(0, 0, 3.0);
    triplets_A.emplace_back(1, 1, 5.0);
    A.setFromTriplets(triplets_A.begin(), triplets_A.end());

    Eigen::SparseMatrix<Scalar> M(2, 2);
    std::vector<Eigen::Triplet<Scalar>> triplets_M;
    triplets_M.emplace_back(0, 0, 1.0);
    triplets_M.emplace_back(1, 1, 1.0);
    M.setFromTriplets(triplets_M.begin(), triplets_M.end());

    auto result = lagrange::solver::generalized_selfadjoint_eigen_smallest<Scalar>(A, M, 1);

    REQUIRE(result.is_successful());
    REQUIRE(result.num_converged >= 1);
    REQUIRE(result.eigenvalues.size() >= 1);
    REQUIRE(result.eigenvectors.rows() == 2);

    // The smallest eigenvalue should be 3
    REQUIRE_THAT(std::abs(result.eigenvalues(0)), Catch::Matchers::WithinAbs(3.0, 1e-6));
}

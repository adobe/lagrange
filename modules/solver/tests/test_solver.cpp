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

#include <catch2/catch_test_macros.hpp>

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

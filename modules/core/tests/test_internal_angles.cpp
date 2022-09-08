/*
 * Copyright 2022 Adobe. All rights reserved.
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
#include <lagrange/internal/internal_angles.h>
#include <igl/internal_angles.h>
#include <lagrange/testing/common.h>

#include <spdlog/fmt/ostr.h>

TEST_CASE("Internal angles - precision", "[core]")
{
    for (double x : {1.0000001, 1.00000001, 1.000000001}) {
        Eigen::MatrixXd V = (Eigen::MatrixXd(3, 3) << 0, 0, 0, 1, 1, 0, x, 1, 0).finished();
        Eigen::MatrixXi F = (Eigen::MatrixXi(1, 3) << 0, 1, 2).finished();
        lagrange::logger().info("##### x = {}", x);

        Eigen::MatrixXd A;
        lagrange::internal::internal_angles(V, F, A);
        lagrange::logger().info("A: {}", A);
        REQUIRE(A.array().isFinite().all());

        Eigen::MatrixXf fV = V.cast<float>();
        Eigen::MatrixXf fA;
        lagrange::internal::internal_angles(fV, F, fA);
        lagrange::logger().info("fA: {}", fA);
        REQUIRE(fA.array().isFinite().all());

        if (x == 0000001) {
            REQUIRE(A.isApprox(fA.cast<double>()));
        }
    }
}

TEST_CASE("Internal angles - flipped", "[core]")
{
    Eigen::MatrixXd V = (Eigen::MatrixXd(3, 3) << 0, 0, 0, 0, 1, 0, 1, 0, 0).finished();
    Eigen::MatrixXi F = (Eigen::MatrixXi(1, 3) << 0, 1, 2).finished();

    Eigen::MatrixXd A, K;
    lagrange::internal::internal_angles(V, F, A);
    lagrange::logger().info("A: {}", A);
    REQUIRE((A.array() > 0).all());
}

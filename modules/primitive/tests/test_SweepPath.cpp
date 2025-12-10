/*
 * Copyright 2021 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/common.h>
#include <lagrange/internal/constants.h>
#include <lagrange/primitive/SweepPath.h>
#include <lagrange/testing/common.h>
#include <catch2/catch_approx.hpp>

TEST_CASE("SweepPath", "[primitive][sweep_path]")
{
    using namespace lagrange;
    using Scalar = float;
    using VertexArray = Eigen::Matrix<Scalar, Eigen::Dynamic, 3>;

    SECTION("Linear")
    {
        Eigen::Matrix<Scalar, 1, 3> dir(0, 0, 1);
        auto sweep_path = std::make_unique<primitive::LinearSweepPath<Scalar>>(dir);
        sweep_path->set_depth_begin(0);
        sweep_path->set_depth_end(2);
        sweep_path->set_twist_begin(0);
        sweep_path->set_twist_end(2 * lagrange::internal::pi);
        sweep_path->set_num_samples(10);
        sweep_path->initialize();

        const auto& transforms = sweep_path->get_transforms();
        REQUIRE(transforms.size() == 10);
    }

    SECTION("Polyline")
    {
        VertexArray polyline(5, 3);
        polyline << 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0;
        auto sweep_path = std::make_unique<primitive::PolylineSweepPath<VertexArray>>(polyline);
        sweep_path->initialize();

        const auto& transforms = sweep_path->get_transforms();
        REQUIRE(transforms.size() > 1);

        REQUIRE(
            (transforms.front().matrix() - transforms.back().matrix()).norm() ==
            Catch::Approx(0).margin(1e-6));
    }
}

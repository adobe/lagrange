/*
 * Copyright 2020 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/testing/common.h>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>

#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/compute_corner_normal.h>
#include <lagrange/create_mesh.h>


#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
TEST_CASE("legacy::compute_corner_normal", "[mesh][triangle][attribute][corner_normal][legacy]")
{
    using namespace lagrange;
    auto mesh = create_cube();

    SECTION("Keep edge sharp")
    {
        compute_corner_normal(*mesh, M_PI * 0.25);
        REQUIRE(mesh->has_corner_attribute("normal"));

        const auto& corner_normals = mesh->get_corner_attribute("normal");
        REQUIRE(corner_normals.rows() == 36);
        REQUIRE(
            corner_normals.rowwise().template lpNorm<Eigen::Infinity>().minCoeff() ==
            Catch::Approx(1.0));
    }

    SECTION("Smooth edge")
    {
        compute_corner_normal(*mesh, M_PI);
        REQUIRE(mesh->has_corner_attribute("normal"));

        const auto& corner_normals = mesh->get_corner_attribute("normal");
        REQUIRE(corner_normals.rows() == 36);
        REQUIRE(corner_normals.rowwise().template lpNorm<Eigen::Infinity>().maxCoeff() < 1.0);
    }
}
#endif

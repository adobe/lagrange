/*
 * Copyright 2018 Adobe. All rights reserved.
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

#include <Eigen/Core>
#include <iostream>

#include <lagrange/create_mesh.h>
#include <lagrange/extract_boundary_loops.h>

TEST_CASE("ExtractBoundaryLoops", "[mesh][boundary]")
{
    using namespace lagrange;

    SECTION("no boundary")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 3> vertices(4, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0;
        Eigen::Matrix<int, Eigen::Dynamic, 3> facets(4, 3);
        facets << 0, 2, 1, 0, 2, 3, 0, 1, 3, 1, 2, 3;

        auto mesh = create_mesh(vertices, facets);
        const auto loops = extract_boundary_loops(*mesh);

        REQUIRE(loops.size() == 0);
    }

    SECTION("single triangle")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 2> vertices(3, 2);
        vertices << 0.0, 0.0, 1.0, 0.0, 0.0, 1.0;
        Eigen::Matrix<int, Eigen::Dynamic, 3> facets(1, 3);
        facets << 0, 1, 2;

        auto mesh = create_mesh(vertices, facets);
        const auto loops = extract_boundary_loops(*mesh);

        REQUIRE(loops.size() == 1);
        REQUIRE(loops[0].size() == 4);
    }

    SECTION("double loops")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 2> vertices(6, 2);
        vertices << 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0, 2.0, 0.0, 0.0, 2.0;
        Eigen::Matrix<int, Eigen::Dynamic, 3> facets(2, 3);
        facets << 0, 1, 2, 3, 4, 5;

        auto mesh = create_mesh(vertices, facets);
        const auto loops = extract_boundary_loops(*mesh);

        REQUIRE(loops.size() == 2);
        REQUIRE(loops[0].size() == 4);
        REQUIRE(loops[1].size() == 4);
    }

    SECTION("complex loops")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 2> vertices(5, 2);
        vertices << 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 2.0, 0.0, 0.0, 2.0;
        Eigen::Matrix<int, Eigen::Dynamic, 3> facets(2, 3);
        facets << 0, 1, 2, 2, 3, 4;

        auto mesh = create_mesh(vertices, facets);
        LA_REQUIRE_THROWS(extract_boundary_loops(*mesh));
    }
}

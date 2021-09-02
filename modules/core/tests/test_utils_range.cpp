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

#include <lagrange/Mesh.h>
#include <lagrange/create_mesh.h>
#include <lagrange/utils/range.h>

using namespace lagrange;

TEST_CASE("range", "[range]")
{
    int count = 0;
    int sum = 0;

    Vertices3D vertices(4, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0;
    Triangles facets(2, 3);
    facets << 0, 1, 2, 2, 1, 3;
    auto mesh = create_mesh(vertices, facets);

    std::vector<int> active = {1};

    SECTION("")
    {
        for (int i : range(5)) {
            ++count;
            sum += i;
            REQUIRE(i < 5);
            REQUIRE(i >= 0);
        }
        REQUIRE(count == 5);
        REQUIRE(sum == 10);
    }

    SECTION("")
    {
        for (int i : range(0)) {
            ++count;
            sum += i;
        }
        REQUIRE(count == 0);
        REQUIRE(sum == 0);
    }

    SECTION("")
    {
        for (int i : range(-1)) { // range(-1) is allowed
            ++count;
            sum += i;
        }
        REQUIRE(count == 0);
        REQUIRE(sum == 0);
    }

    SECTION("")
    {
        for (int i : range_sparse(3, active)) {
            ++count;
            sum += i;
        }
        REQUIRE(count == 1);
        REQUIRE(sum == 1);
    }

    SECTION("")
    {
        for (auto f : range_facets(*mesh)) {
            ++count;
            sum += f;
        }
        REQUIRE(count == 2);
        REQUIRE(sum == 1); // 0 + 1
    }

    SECTION("")
    {
        for (auto f : range_facets(*mesh, active)) {
            ++count;
            REQUIRE(f == 1); // loop body should only run once
        }
        REQUIRE(count == 1);
    }

    SECTION("")
    {
        active = {0, 1};
        for (auto f : range_facets(*mesh, active)) {
            ++count;
            sum += f;
        }
        REQUIRE(count == 2);
        REQUIRE(sum == 1);
    }

    SECTION("")
    {
        for (auto v : range_vertices(*mesh)) {
            ++count;
            sum += v;
        }
        REQUIRE(count == 4);
        REQUIRE(sum == 6);
    }

    SECTION("")
    {
        active = {0, 1};
        for (auto v : range_vertices(*mesh, active)) {
            ++count;
            sum += v;
        }
        REQUIRE(count == 2);
        REQUIRE(sum == 1);
    }

    SECTION("")
    {
        auto foo = []() -> std::vector<int> { return {0, 1, 2}; };
        foo();
        // The following statements should *NOT* compile:
        // range_sparse(0, {});
        // range_sparse(0, foo());
        // range_sparse(0, std::move(active));

        // range_vertices(*mesh, {});
        // range_vertices(*mesh, foo());
        // range_vertices(*mesh, std::move(active));

        // range_facets(*mesh, {});
        // range_facets(*mesh, foo());
        // range_facets(*mesh, std::move(active));
    }

    SECTION("row range")
    {
        int i = 0;

        SECTION("vertices")
        {
            for (const auto row : row_range(vertices)) {
                REQUIRE((row.array() == vertices.row(i).array()).all());
                i++;
            }
            REQUIRE(i == vertices.rows());
        }

        SECTION("facets")
        {
            for (const auto row : row_range(facets)) {
                REQUIRE((row.array() == facets.row(i).array()).all());
                i++;
            }
            REQUIRE(i == facets.rows());
        }

        SECTION("Column major matrix")
        {
            Eigen::MatrixXi M(3, 3);
            M << 1, 2, 3, 4, 5, 6, 7, 8, 9;
            for (const auto row : row_range(M)) {
                REQUIRE((row.array() == M.row(i).array()).all());
                i++;
            }
            REQUIRE(i == 3);
        }

        SECTION("Empty matrix")
        {
            Eigen::MatrixXi Z(0, 3);
            for (const auto row : row_range(Z)) {
                REQUIRE((row.array() == Z.row(i).array()).all());
                i++;
            }
            REQUIRE(i == 0);
        }

        SECTION("Empty matrix 2")
        {
            Eigen::MatrixXi Z(3, 0);
            for (const auto row : row_range(Z)) {
                REQUIRE((row.array() == Z.row(i).array()).all());
                i++;
            }
            REQUIRE(i == 3);
        }
    }
}

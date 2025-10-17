/*
 * Copyright 2019 Adobe. All rights reserved.
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
        for (auto f : range(mesh->get_num_facets())) {
            ++count;
            sum += f;
        }
        REQUIRE(count == 2);
        REQUIRE(sum == 1); // 0 + 1
    }

    SECTION("")
    {
        for (auto f : active) {
            ++count;
            REQUIRE(f == 1); // loop body should only run once
        }
        REQUIRE(count == 1);
    }

    SECTION("")
    {
        active = {0, 1};
        for (auto f : active) {
            ++count;
            sum += f;
        }
        REQUIRE(count == 2);
        REQUIRE(sum == 1);
    }

    SECTION("")
    {
        for (auto v : range(mesh->get_num_vertices())) {
            ++count;
            sum += v;
        }
        REQUIRE(count == 4);
        REQUIRE(sum == 6);
    }

    SECTION("")
    {
        active = {0, 1};
        for (auto v : active) {
            ++count;
            sum += v;
        }
        REQUIRE(count == 2);
        REQUIRE(sum == 1);
    }

    SECTION("row range")
    {
        int i = 0;

        SECTION("vertices")
        {
            for (const auto row : vertices.rowwise()) {
                REQUIRE((row.array() == vertices.row(i).array()).all());
                i++;
            }
            REQUIRE(i == vertices.rows());
        }

        SECTION("facets")
        {
            for (const auto row : facets.rowwise()) {
                REQUIRE((row.array() == facets.row(i).array()).all());
                i++;
            }
            REQUIRE(i == facets.rows());
        }

        SECTION("Column major matrix")
        {
            Eigen::MatrixXi M(3, 3);
            M << 1, 2, 3, 4, 5, 6, 7, 8, 9;
            for (const auto row : M.rowwise()) {
                REQUIRE((row.array() == M.row(i).array()).all());
                i++;
            }
            REQUIRE(i == 3);
        }

        SECTION("Empty matrix")
        {
            Eigen::MatrixXi Z(0, 3);
            for (const auto row : Z.rowwise()) {
                REQUIRE((row.array() == Z.row(i).array()).all());
                i++;
            }
            REQUIRE(i == 0);
        }

#if EIGEN_VERSION_AT_LEAST(3, 4, 1)
        // Eigen 3.4 has UBSan issue when iterating over empty matrices:
        // https://godbolt.org/z/1aPxPcWvq
        SECTION("Empty matrix 2")
        {
            Eigen::MatrixXi Z(3, 0);
            for (const auto row : Z.rowwise()) {
                REQUIRE((row.array() == Z.row(i).array()).all());
                i++;
            }
            REQUIRE(i == 3);
        }
#endif
    }
}

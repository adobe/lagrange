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
#include <catch2/catch_approx.hpp>

#include <limits>

#include <lagrange/bvh/EdgeAABBTree.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/utils/range.h>

TEST_CASE("bvh/EdgeAABBTree", "[bvh][aabb][edge]")
{
    using namespace lagrange;

    SECTION("Simple 3D") {
        using Scalar = float;
        using Index = int;
        using Point = Eigen::Matrix<Scalar, 1, 3>;
        using VertexArray = Eigen::Matrix<Scalar, Eigen::Dynamic, 3>;
        using EdgeArray = Eigen::Matrix<Index, Eigen::Dynamic, 2>;

        VertexArray vertices(3, 3);
        vertices << 0, 0, 0,
                    1, 0, 0,
                    0, 1, 0;
        EdgeArray edges(3, 2);
        edges << 0, 1, 1, 2, 2, 0;

        bvh::EdgeAABBTree<VertexArray, EdgeArray> aabb(vertices, edges);
        REQUIRE(!aabb.empty());

        Index element_id = invalid<Index>();
        Point closest_pt(invalid<Scalar>(), invalid<Scalar>(), invalid<Scalar>());
        Scalar sq_dist = invalid<Scalar>();

        SECTION("Boundary query") {
            Point q(0.1f, 0.0f, 0.0f);
            aabb.get_closest_point(q, element_id, closest_pt, sq_dist);
            REQUIRE(element_id == 0);
            REQUIRE(closest_pt[0] == Catch::Approx(0.1f));
            REQUIRE(closest_pt[1] == Catch::Approx(0.0f));
            REQUIRE(sq_dist == Catch::Approx(0.0f));
        }

        SECTION("Internal query") {
            Point q(0.1f, 0.5f, 0.0f);
            aabb.get_closest_point(q, element_id, closest_pt, sq_dist);
            REQUIRE(element_id == 2);
            REQUIRE(closest_pt[0] == Catch::Approx(0.0f));
            REQUIRE(closest_pt[1] == Catch::Approx(0.5f));
            REQUIRE(sq_dist == Catch::Approx(0.01f));
        }

        SECTION("External query") {
            Point q(-0.1f, 0.5f, 0.0f);
            aabb.get_closest_point(q, element_id, closest_pt, sq_dist);
            REQUIRE(element_id == 2);
            REQUIRE(closest_pt[0] == Catch::Approx(0.0f));
            REQUIRE(closest_pt[1] == Catch::Approx(0.5f));
            REQUIRE(sq_dist == Catch::Approx(0.01f));
        }
    }

    SECTION("Simple 2D") {
        using Scalar = float;
        using Index = int;
        using Point = Eigen::Matrix<Scalar, 1, 2>;
        using VertexArray = Eigen::Matrix<Scalar, Eigen::Dynamic, 2>;
        using EdgeArray = Eigen::Matrix<Index, Eigen::Dynamic, 2>;

        VertexArray vertices(3, 2);
        vertices << 0, 0,
                    1, 0,
                    0, 1;
        EdgeArray edges(3, 2);
        edges << 0, 1, 1, 2, 2, 0;

        bvh::EdgeAABBTree<VertexArray, EdgeArray, 2> aabb(vertices, edges);
        REQUIRE(!aabb.empty());

        Index element_id = invalid<Index>();
        Point closest_pt(invalid<Scalar>(), invalid<Scalar>());
        Scalar sq_dist = invalid<Scalar>();

        SECTION("Boundary query") {
            Point q(0.1f, 0.0f);
            aabb.get_closest_point(q, element_id, closest_pt, sq_dist);
            REQUIRE(element_id == 0);
            REQUIRE(closest_pt[0] == Catch::Approx(0.1f));
            REQUIRE(closest_pt[1] == Catch::Approx(0.0f));
            REQUIRE(sq_dist == Catch::Approx(0.0f));
        }

        SECTION("Internal query") {
            Point q(0.1f, 0.5f);
            aabb.get_closest_point(q, element_id, closest_pt, sq_dist);
            REQUIRE(element_id == 2);
            REQUIRE(closest_pt[0] == Catch::Approx(0.0f));
            REQUIRE(closest_pt[1] == Catch::Approx(0.5f));
            REQUIRE(sq_dist == Catch::Approx(0.01f));
        }

        SECTION("External query") {
            Point q(-0.1f, 0.5f);
            aabb.get_closest_point(q, element_id, closest_pt, sq_dist);
            REQUIRE(element_id == 2);
            REQUIRE(closest_pt[0] == Catch::Approx(0.0f));
            REQUIRE(closest_pt[1] == Catch::Approx(0.5f));
            REQUIRE(sq_dist == Catch::Approx(0.01f));
        }
    }
}

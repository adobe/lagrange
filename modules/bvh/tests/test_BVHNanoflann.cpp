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
#include <catch2/catch_approx.hpp>

#include <limits>

#include <lagrange/bvh/BVHNanoflann.h>
#include <lagrange/common.h>
#include <lagrange/utils/range.h>

TEST_CASE("BVHNanoflann", "[bvh][nanoflann]")
{
    using namespace lagrange;
    using VertexArray = Eigen::Matrix<double, Eigen::Dynamic, 3, Eigen::RowMajor>;

    bvh::BVHNanoflann<VertexArray> engine;
    REQUIRE(engine.does_support_query_in_sphere_neighbours());
    REQUIRE(engine.does_support_query_k_nearest_neighbours());
    REQUIRE(engine.does_support_query_closest_point());

    SECTION("Simple")
    {
        VertexArray vertices(3, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;
        engine.build(vertices);

        SECTION("Query on input point")
        {
            for (const auto p : vertices.rowwise()) {
                auto r = engine.query_closest_point(p);
                REQUIRE(r.closest_vertex_idx >= 0);
                REQUIRE(r.closest_vertex_idx < vertices.rows());
                REQUIRE(r.closest_point == vertices.row(r.closest_vertex_idx));
                REQUIRE(r.squared_distance == Catch::Approx(0.0));
            }
        }

        SECTION("Query on nearby points")
        {
            auto r0 = engine.query_closest_point({0.1, 0.0, 0.0});
            REQUIRE(r0.closest_vertex_idx == 0);
            REQUIRE(r0.closest_point == vertices.row(0));
            REQUIRE(r0.squared_distance == Catch::Approx(0.01));

            auto r1 = engine.query_closest_point({0.4, 0.0, 0.0});
            REQUIRE(r1.closest_vertex_idx == 0);
            REQUIRE(r1.closest_point == vertices.row(0));
            REQUIRE(r1.squared_distance == Catch::Approx(0.16));

            auto r2 = engine.query_closest_point({0.6, 0.0, 0.0});
            REQUIRE(r2.closest_vertex_idx == 1);
            REQUIRE(r2.closest_point == vertices.row(1));
            REQUIRE(r2.squared_distance == Catch::Approx(0.16));

            auto r3 = engine.query_closest_point({0.0, 0.9, 0.0});
            REQUIRE(r3.closest_vertex_idx == 2);
            REQUIRE(r3.closest_point == vertices.row(2));
            REQUIRE(r3.squared_distance == Catch::Approx(0.01));
        }

        SECTION("Batch query")
        {
            auto r = engine.batch_query_closest_point(vertices);
            REQUIRE(r.size() == 3);
            REQUIRE(r[0].closest_vertex_idx == 0);
            REQUIRE(r[1].closest_vertex_idx == 1);
            REQUIRE(r[2].closest_vertex_idx == 2);

            REQUIRE(r[0].closest_point == vertices.row(0));
            REQUIRE(r[1].closest_point == vertices.row(1));
            REQUIRE(r[2].closest_point == vertices.row(2));

            REQUIRE(r[0].squared_distance == Catch::Approx(0.0));
            REQUIRE(r[1].squared_distance == Catch::Approx(0.0));
            REQUIRE(r[2].squared_distance == Catch::Approx(0.0));
        }

        SECTION("Batch query 2")
        {
            VertexArray query_pts(3, 3);
            query_pts = vertices;
            query_pts.col(2).setOnes();

            auto r = engine.batch_query_closest_point(query_pts);
            REQUIRE(r.size() == 3);
            REQUIRE(r[0].closest_vertex_idx == 0);
            REQUIRE(r[1].closest_vertex_idx == 1);
            REQUIRE(r[2].closest_vertex_idx == 2);

            REQUIRE(r[0].closest_point == vertices.row(0));
            REQUIRE(r[1].closest_point == vertices.row(1));
            REQUIRE(r[2].closest_point == vertices.row(2));

            REQUIRE(r[0].squared_distance == Catch::Approx(1.0));
            REQUIRE(r[1].squared_distance == Catch::Approx(1.0));
            REQUIRE(r[2].squared_distance == Catch::Approx(1.0));
        }
    }

    SECTION("Query by radius")
    {
        VertexArray vertices(5, 3);
        vertices << 0.0, 0.0, 0.0, 1e-12, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 1e12, 1e12, 1e12;
        engine.build(vertices);

        SECTION("Near zero radius")
        {
            const auto r = engine.query_in_sphere_neighbours(
                {0.0, 0.0, 0.0},
                sqrt(std::numeric_limits<VertexArray::Scalar>::min()));
            REQUIRE(r.size() == 1);
            const auto& r0 = r[0];

            REQUIRE(r0.closest_vertex_idx == 0);
            REQUIRE(r0.closest_point == vertices.row(0));
            REQUIRE(r0.squared_distance == Catch::Approx(0.0));
        }

        SECTION("Small radius")
        {
            const auto r = engine.query_in_sphere_neighbours({0.0, 0.0, 0.0}, 0.1);
            REQUIRE(r.size() == 2);

            const auto& r0 = r[0];
            const auto& r1 = r[1];

            REQUIRE(r0.closest_vertex_idx == 0);
            REQUIRE(r0.closest_point == vertices.row(0));
            REQUIRE(r0.squared_distance == Catch::Approx(0.0));

            REQUIRE(r1.closest_vertex_idx == 1);
            REQUIRE(r1.closest_point == vertices.row(1));
            REQUIRE(r1.squared_distance <= Catch::Approx(0.01));
        }

        SECTION("Large radius")
        {
            const auto r = engine.query_in_sphere_neighbours({0.0, 0.0, 0.0}, 2);
            REQUIRE(r.size() == 4);

            const auto& r0 = r[0];
            const auto& r1 = r[1];
            const auto& r2 = r[2];
            const auto& r3 = r[3];

            REQUIRE(r0.closest_vertex_idx == 0);
            REQUIRE(r0.closest_point == vertices.row(0));
            REQUIRE(r0.squared_distance == Catch::Approx(0.0));

            REQUIRE(r1.closest_vertex_idx == 1);
            REQUIRE(r1.closest_point == vertices.row(1));
            REQUIRE(r1.squared_distance <= Catch::Approx(0.01));

            REQUIRE(r2.closest_vertex_idx == 2);
            REQUIRE(r2.closest_point == vertices.row(2));
            REQUIRE(r2.squared_distance <= Catch::Approx(1.0));

            REQUIRE(r3.closest_vertex_idx == 3);
            REQUIRE(r3.closest_point == vertices.row(3));
            REQUIRE(r3.squared_distance <= Catch::Approx(2.0));
        }

        SECTION("Crazy large radius")
        {
            const auto r = engine.query_in_sphere_neighbours(
                {0.0, 0.0, 0.0},
                sqrt(std::numeric_limits<VertexArray::Scalar>::max()));
            REQUIRE(r.size() == 5);

            const auto& r0 = r[0];
            const auto& r1 = r[1];
            const auto& r2 = r[2];
            const auto& r3 = r[3];
            const auto& r4 = r[4];

            REQUIRE(r0.closest_vertex_idx == 0);
            REQUIRE(r0.closest_point == vertices.row(0));
            REQUIRE(r0.squared_distance == Catch::Approx(0.0));

            REQUIRE(r1.closest_vertex_idx == 1);
            REQUIRE(r1.closest_point == vertices.row(1));
            REQUIRE(r1.squared_distance <= Catch::Approx(0.01));

            REQUIRE(r2.closest_vertex_idx == 2);
            REQUIRE(r2.closest_point == vertices.row(2));
            REQUIRE(r2.squared_distance <= Catch::Approx(1.0));

            REQUIRE(r3.closest_vertex_idx == 3);
            REQUIRE(r3.closest_point == vertices.row(3));
            REQUIRE(r3.squared_distance <= Catch::Approx(2.0));

            REQUIRE(r4.closest_vertex_idx == 4);
            REQUIRE(r4.closest_point == vertices.row(4));
            REQUIRE(r4.squared_distance <= Catch::Approx(vertices.row(4).squaredNorm()));
        }
    }

    SECTION("Query k nearest neighbours")
    {
        VertexArray vertices(4, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 0.0;
        engine.build(vertices);

        SECTION("Query input point, k=1")
        {
            for (const auto p : vertices.rowwise()) {
                auto rs = engine.query_k_nearest_neighbours(p, 1);
                REQUIRE(rs.size() == 1);
                REQUIRE(rs[0].closest_vertex_idx >= 0);
                REQUIRE(rs[0].closest_vertex_idx < vertices.rows());
                REQUIRE(rs[0].closest_point == vertices.row(rs[0].closest_vertex_idx));
                REQUIRE(rs[0].squared_distance == Catch::Approx(0.0));
            }
        }

        struct KNNTestResult
        {
            int idx;
            double squared_dist;
        };

        using ClosestPoint = bvh::BVH<VertexArray, Triangles>::ClosestPoint;

        auto test_knn_results = [&vertices](
                                    const std::vector<KNNTestResult>& expected,
                                    const std::vector<ClosestPoint>& actual) {
            for (const auto& r : actual) {
                bool in_expected_list = false;
                for (const auto& e : expected) {
                    if (e.idx != r.closest_vertex_idx) {
                        continue;
                    }
                    REQUIRE(r.closest_point == vertices.row(e.idx));
                    REQUIRE(r.squared_distance == Catch::Approx(e.squared_dist));
                    in_expected_list = true;
                    break;
                }
                REQUIRE(in_expected_list);
            }
        };

        SECTION("Query nearby points, k=2")
        {
            auto r0 = engine.query_k_nearest_neighbours({0.1, 0.0, 0.0}, 2);
            std::vector<KNNTestResult> r0_expected = {{0, 0.01}, {1, 0.81}};
            test_knn_results(r0_expected, r0);

            auto r1 = engine.query_k_nearest_neighbours({0.0, 0.4, 0.0}, 2);
            std::vector<KNNTestResult> r1_expected = {{0, 0.16}, {3, 0.36}};
            test_knn_results(r1_expected, r1);

            auto r2 = engine.query_k_nearest_neighbours({0.7, 1.0, 0.0}, 2);
            std::vector<KNNTestResult> r2_expected = {{2, 0.09}, {3, 0.49}};
            test_knn_results(r2_expected, r2);

            auto r3 = engine.query_k_nearest_neighbours({1.0, 0.2, 0.0}, 2);
            std::vector<KNNTestResult> r3_expected = {{1, 0.04}, {2, 0.64}};
            test_knn_results(r3_expected, r3);
        }

        SECTION("Increasing k")
        {
            Eigen::RowVector3d p;
            p << 0.1, 0.2, 0.0;

            auto r0 = engine.query_k_nearest_neighbours(p, 1);
            std::vector<KNNTestResult> r0_expected = {{0, 0.05}};
            test_knn_results(r0_expected, r0);

            auto r1 = engine.query_k_nearest_neighbours(p, 2);
            std::vector<KNNTestResult> r1_expected = {{0, 0.05}, {3, 0.65}};
            test_knn_results(r1_expected, r1);

            auto r2 = engine.query_k_nearest_neighbours(p, 3);
            std::vector<KNNTestResult> r2_expected = {{0, 0.05}, {3, 0.65}, {1, 0.85}};
            test_knn_results(r2_expected, r2);

            auto r3 = engine.query_k_nearest_neighbours(p, 4);
            std::vector<KNNTestResult> r3_expected = {{0, 0.05}, {3, 0.65}, {1, 0.85}, {2, 1.45}};
            test_knn_results(r3_expected, r3);
        }
    }
}

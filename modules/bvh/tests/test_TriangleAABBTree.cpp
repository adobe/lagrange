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
#include <lagrange/bvh/AABBIGL.h>
#include <lagrange/bvh/TriangleAABBTree.h>
#include <lagrange/reorder_mesh.h>
#include <lagrange/testing/common.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/views.h>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("TriangleAABBTree", "[bvh][aabb][triangle]")
{
    using namespace lagrange;
    using Scalar = float;
    using Index = uint32_t;

    SECTION("Single triangle 3D")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);

        bvh::TriangleAABBTree<Scalar, Index> aabb(mesh);
        REQUIRE(!aabb.empty());

        Eigen::RowVector3f q(0.1f, 0.1f, 0.0f);
        Index triangle_id = invalid<Index>();
        Eigen::RowVector3f closest_pt;
        Scalar sq_dist = invalid<Scalar>();
        aabb.get_closest_point(q, triangle_id, closest_pt, sq_dist);

        REQUIRE(triangle_id == 0);
        REQUIRE_THAT(sq_dist, Catch::Matchers::WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(closest_pt[0], Catch::Matchers::WithinAbs(q[0], 1e-6f));
        REQUIRE_THAT(closest_pt[1], Catch::Matchers::WithinAbs(q[1], 1e-6f));
        REQUIRE_THAT(closest_pt[2], Catch::Matchers::WithinAbs(q[2], 1e-6f));
    }

    SECTION("Two triangles 3D")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 3, 2);

        bvh::TriangleAABBTree<Scalar, Index> aabb(mesh);
        REQUIRE(!aabb.empty());

        Eigen::RowVector3f q(0.5f, 0.5f, 0.1f);
        Scalar r = 0.2f;
        size_t count = 0;
        aabb.foreach_triangle_in_radius(
            q,
            r * r,
            [&](Scalar sq_dist, Index triangle_id, const Eigen::RowVector3f& pt) {
                REQUIRE(triangle_id < mesh.get_num_facets());
                REQUIRE(sq_dist < r * r);
                REQUIRE_THAT(pt[0], Catch::Matchers::WithinAbs(q[0], 1e-6f));
                REQUIRE_THAT(pt[1], Catch::Matchers::WithinAbs(q[1], 1e-6f));
                REQUIRE_THAT(pt[2], Catch::Matchers::WithinAbs(0, 1e-6f));
                count++;
            });
        REQUIRE(count == 2);
    }

    SECTION("2D mesh")
    {
        SurfaceMesh<Scalar, Index> mesh(2);
        mesh.add_vertex({0, 0});
        mesh.add_vertex({1, 0});
        mesh.add_vertex({0, 1});
        mesh.add_vertex({1, 1});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 3, 2);

        bvh::TriangleAABBTree<Scalar, Index, 2> aabb(mesh);
        REQUIRE(!aabb.empty());

        Eigen::RowVector2f q(-0.5f, -0.5f);
        Index triangle_id = invalid<Index>();
        Eigen::RowVector2f closest_pt;
        Scalar sq_dist = invalid<Scalar>();
        aabb.get_closest_point(q, triangle_id, closest_pt, sq_dist);

        REQUIRE(triangle_id == 0);
        REQUIRE_THAT(sq_dist, Catch::Matchers::WithinAbs(0.5f, 1e-6f));
        REQUIRE_THAT(closest_pt[0], Catch::Matchers::WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(closest_pt[1], Catch::Matchers::WithinAbs(0.0f, 1e-6f));
    }
}

TEST_CASE("TriangleAABBTree benchmark", "[bvh][aabb][!benchmark]")
{
    using namespace lagrange;
    using Scalar = float;
    using Index = uint32_t;
    using VertexArray = Eigen::Matrix<Scalar, Eigen::Dynamic, 3, Eigen::RowMajor>;
    using TriangleArray = Eigen::Matrix<Index, Eigen::Dynamic, 3, Eigen::RowMajor>;

    auto ref_mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");
    auto input_mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");
    reorder_mesh(input_mesh, ReorderingMethod::Hilbert);
    auto vertices = vertex_view(input_mesh);
    auto facets = facet_view(input_mesh);

    BENCHMARK("build AABB tree")
    {
        bvh::TriangleAABBTree<Scalar, Index> aabb(input_mesh);
        return aabb.empty();
    };
    bvh::TriangleAABBTree<Scalar, Index> aabb(input_mesh);

    bvh::AABBIGL<VertexArray, TriangleArray> igl_aabb;
    BENCHMARK("build AABBIGL tree")
    {
        igl_aabb.build(vertices, facets);
        return igl_aabb.does_support_query_closest_point();
    };
    igl_aabb.build(vertices, facets);

    Eigen::RowVector3f q(0.1f, 0.2f, 0.3f);
    Index triangle_id = invalid<Index>();
    Eigen::RowVector3f closest_pt;
    Scalar sq_dist = invalid<Scalar>();

    auto ref_vertices = vertex_view(ref_mesh);
    const Index N = 1000;
    BENCHMARK("query AABB tree")
    {
        Scalar total_dist = 0;
        for (Index i = 0; i < N; i++) {
            q = ref_vertices.row(i).array() + 1;
            aabb.get_closest_point(q, triangle_id, closest_pt, sq_dist);
            total_dist += sq_dist;
        }
        return total_dist;
    };

    BENCHMARK("query AABBIGL tree")
    {
        Scalar total_dist = 0;
        for (Index i = 0; i < N; i++) {
            q = ref_vertices.row(i).array() + 1;
            auto cp = igl_aabb.query_closest_point(q);
            total_dist += cp.squared_distance;
        }
        return total_dist;
    };

    aabb.get_closest_point(q, triangle_id, closest_pt, sq_dist);
    auto cp = igl_aabb.query_closest_point(q);
    INFO("AABB closest sq_dist: " << sq_dist);
    INFO("IGL closest sq_dist: " << cp.squared_distance);
    REQUIRE_THAT(cp.squared_distance, Catch::Matchers::WithinAbs(sq_dist, 1e-6f));
}

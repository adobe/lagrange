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

#include <lagrange/testing/common.h>

#include <lagrange/Logger.h>
#include <lagrange/bvh/weld_vertices.h>
#include <lagrange/combine_meshes.h>
#include <lagrange/compute_edge_lengths.h>
#include <lagrange/topology.h>
#include <lagrange/views.h>

#include <catch2/benchmark/catch_benchmark.hpp>

TEST_CASE("weld_vertices", "[bvh][surface]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SECTION("Simple")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(3, 4, 5);

        REQUIRE(mesh.get_num_vertices() == 6);
        REQUIRE(mesh.get_num_facets() == 2);

        bvh::weld_vertices(mesh);

        REQUIRE(mesh.get_num_vertices() == 4);
        REQUIRE(mesh.get_num_facets() == 2);
    }

    SECTION("Simple 2D")
    {
        SurfaceMesh<Scalar, Index> mesh(2);
        mesh.add_vertex({0, 0});
        mesh.add_vertex({1, 0});
        mesh.add_vertex({1, 1});
        mesh.add_vertex({0, 0});
        mesh.add_vertex({1, 1});
        mesh.add_vertex({0, 1});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(3, 4, 5);

        REQUIRE(mesh.get_num_vertices() == 6);
        REQUIRE(mesh.get_num_facets() == 2);

        bvh::weld_vertices(mesh);

        REQUIRE(mesh.get_num_vertices() == 4);
        REQUIRE(mesh.get_num_facets() == 2);
    }

    SECTION("non-manifold")
    {
        SurfaceMesh<Scalar, Index> mesh;
        // clang-format off
        mesh.add_vertices(9, {
                0.0, 0.0, 0.0,
                1.0, 0.0, 0.0,
                0.0, 1.0, 0.0,
                0.0, 0.0, 0.0,
                1.0, 0.0, 0.0,
                0.0, 2.0, 0.0,
                0.0, 0.0, 0.0,
                1.0, 0.0, 0.0,
                0.0, 3.0, 0.0
        });
        // clang-format on
        mesh.add_triangles(3, {0, 1, 2, 3, 4, 5, 6, 7, 8});

        REQUIRE(mesh.get_num_vertices() == 9);
        REQUIRE(mesh.get_num_facets() == 3);

        bvh::WeldOptions options;
        options.boundary_only = true;

        SECTION("small radius")
        {
            options.radius = 0.1;
            bvh::weld_vertices(mesh, options);
            REQUIRE(mesh.get_num_vertices() == 5);
            REQUIRE(mesh.get_num_facets() == 3);
            REQUIRE(!is_vertex_manifold(mesh));
        }

        SECTION("large radius")
        {
            options.radius = 10;
            bvh::weld_vertices(mesh, options);
            // All vertices got mapping to a single vertex
            REQUIRE(mesh.get_num_vertices() == 1);
            // Resulting in 3 degenerate facets
            REQUIRE(mesh.get_num_facets() == 3);
            REQUIRE(!is_vertex_manifold(mesh));
        }

        SECTION("zero radius")
        {
            options.radius = 0.0;
            bvh::weld_vertices(mesh, options);
            REQUIRE(mesh.get_num_vertices() == 9);
            REQUIRE(mesh.get_num_facets() == 3);
        }
    }

    SECTION("interior vertices")
    {
        SurfaceMesh<Scalar, Index> mesh1;
        // clang-format off
        mesh1.add_vertices(5, {
            0.0, 0.0, 0.0,
            1.0, 0.0, 0.0,
            1.0, 1.0, 0.0,
            0.0, 1.0, 0.0,
            0.5, 0.5, 0.0
        });
        // clang-format on
        mesh1.add_triangles(4, {0, 1, 4, 1, 2, 4, 2, 3, 4, 3, 0, 4});

        auto mesh2 = mesh1;
        auto facets = facet_ref(mesh2);
        facets.col(1).swap(facets.col(2));

        auto mesh = combine_meshes({&mesh1, &mesh2});
        REQUIRE(mesh.get_num_vertices() == 10);
        REQUIRE(compute_euler(mesh) == 2);

        bvh::WeldOptions options;
        options.boundary_only = true;
        bvh::weld_vertices(mesh, options);
        REQUIRE(mesh.get_num_vertices() == 6);
        REQUIRE(compute_euler(mesh) == 2);
        REQUIRE(is_vertex_manifold(mesh));
    }
}

TEST_CASE("weld_vertices all", "[bvh][surface]")
{
    using namespace lagrange;
    using Scalar = float;
    using Index = uint32_t;

    auto input_mesh =
        lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");

    bvh::WeldOptions options;
    options.radius = [&] {
        auto mesh = input_mesh;
        auto id = compute_edge_lengths(mesh);
        auto lengths = attribute_vector_view<Scalar>(mesh, id);
        return lengths.maxCoeff() * 1.1f;
    }();
    logger().info("radius: {}", options.radius);

    Index num_before = input_mesh.get_num_vertices();
    bvh::weld_vertices(input_mesh, options);
    Index num_after = input_mesh.get_num_vertices();
    logger().info("weld_vertices all: {} -> {}", num_before, num_after);

    REQUIRE(num_after < num_before);
    REQUIRE(num_after > 4000);
}

TEST_CASE("weld_vertices benchmark", "[weld_vertices][!benchmark]")
{
    using namespace lagrange;
    using Scalar = float;
    using Index = uint32_t;

    const auto input_mesh =
        lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");

    bvh::WeldOptions options;
    options.radius = [&] {
        auto mesh = input_mesh;
        auto id = compute_edge_lengths(mesh);
        auto lengths = attribute_vector_view<Scalar>(mesh, id);
        return lengths.maxCoeff() * 1.1f;
    }();

    BENCHMARK_ADVANCED("weld all vertices")(Catch::Benchmark::Chronometer meter)
    {
        auto mesh = input_mesh;
        meter.measure([&]() {
            bvh::weld_vertices(mesh, options);
            return mesh.get_num_vertices();
        });
    };

    options.boundary_only = true;
    BENCHMARK_ADVANCED("weld boundary")(Catch::Benchmark::Chronometer meter)
    {
        // TODO: Split mesh into disconnected components with open boundaries. Right now this is a no-op.
        auto mesh = input_mesh;
        meter.measure([&]() {
            bvh::weld_vertices(mesh, options);
            return mesh.get_num_vertices();
        });
    };
}

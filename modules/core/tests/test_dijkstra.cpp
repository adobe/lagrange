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

#include <lagrange/testing/common.h>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_approx.hpp>

#include <lagrange/Logger.h>
#include <lagrange/internal/dijkstra.h>
#include <lagrange/mesh_convert.h>
#include <lagrange/views.h>

TEST_CASE("dijkstra", "[surface][internal][utility]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SECTION("quad")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(2, 1, 3);

        Index seed_vertices[] = {0};
        Scalar seed_vertex_distance[] = {0};
        Scalar expected_dist[]{0, 1, 1, 2};

        auto vertices = vertex_view(mesh);
        auto check = [&](Scalar radius, size_t expected_num_vertices_reached) {
            auto dist = [&](Index vi, Index vj) {
                return (vertices.row(vi) - vertices.row(vj)).norm();
            };

            size_t num_vertices_reached = 0;
            auto process = [&](Index vi, Scalar d) -> bool {
                logger().debug("{}: {}", vi, d);
                REQUIRE(d < radius);
                REQUIRE(d == Catch::Approx(expected_dist[vi]));
                num_vertices_reached++;
                return false;
            };

            lagrange::internal::dijkstra<Scalar, Index>(
                mesh,
                seed_vertices,
                seed_vertex_distance,
                radius,
                dist,
                process);
            REQUIRE(expected_num_vertices_reached == num_vertices_reached);
        };

        check(0.1, 1);
        check(1.1, 3);
        check(2.1, 4);
    }

    SECTION("mixed")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_vertex({1, 0, 1});
        mesh.add_vertex({1, 1, 1});
        mesh.add_quad(0, 2, 3, 1);
        mesh.add_triangle(1, 3, 4);
        mesh.add_triangle(4, 3, 5);

        Index seed_vertices[]{0};
        Scalar seed_vertex_distance[]{0};
        Scalar expected_dist[]{0, 1, 1, 2, 2, 3};

        auto vertices = vertex_view(mesh);
        auto check = [&](Scalar radius, size_t expected_num_vertices_reached) {
            auto dist = [&](Index vi, Index vj) {
                return (vertices.row(vi) - vertices.row(vj)).norm();
            };

            size_t num_vertices_reached = 0;
            auto process = [&](Index vi, Scalar d) -> bool {
                logger().debug("{}: {}", vi, d);
                REQUIRE(d < radius);
                REQUIRE(d == Catch::Approx(expected_dist[vi]));
                num_vertices_reached++;
                return false;
            };

            lagrange::internal::dijkstra<Scalar, Index>(
                mesh,
                seed_vertices,
                seed_vertex_distance,
                radius,
                dist,
                process);
            REQUIRE(expected_num_vertices_reached == num_vertices_reached);
        };

        check(0.1, 1);
        check(1.1, 3);
        check(2.1, 5);
        check(3.1, 6);
    }
}

TEST_CASE("dijkstra benchmark", "[surface][utility][internal][!benchmark]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");
    auto vertices = vertex_view(mesh);
    auto dist = [&](Index vi, Index vj) { return (vertices.row(vi) - vertices.row(vj)).norm(); };

    BENCHMARK_ADVANCED("dijkstra")(Catch::Benchmark::Chronometer meter)
    {
        mesh.initialize_edges();
        size_t count = 0;
        Index sources[]{0};
        Scalar source_dist[]{0};
        auto process = [&](Index, Scalar) {
            count++;
            return false;
        };

        meter.measure([&]() {
            lagrange::internal::dijkstra<Scalar, Index>(
                mesh,
                sources,
                source_dist,
                0,
                dist,
                process);
            return count;
        });
    };

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    using MeshType = TriangleMesh3D;
    auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);

    BENCHMARK_ADVANCED("dijkstra legacy")(Catch::Benchmark::Chronometer meter)
    {
        legacy_mesh->initialize_connectivity();
        size_t count = 0;
        auto process = [&](Index, Scalar) {
            count++;
            return false;
        };

        meter.measure([&]() {
            lagrange::internal::dijkstra<MeshType>(*legacy_mesh, {0}, {0}, 0, dist, process);
            return count;
        });
    };
#endif
}

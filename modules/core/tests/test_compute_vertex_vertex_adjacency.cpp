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

#include <lagrange/compute_vertex_vertex_adjacency.h>
#include <lagrange/utils/span.h>

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/Mesh.h>
    #include <lagrange/mesh_convert.h>
#endif

TEST_CASE("compute_vertex_vertex_adjacency", "[surface][adjacency][utilities]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto check_adjacency = [](SurfaceMesh<Scalar, Index>& mesh,
                              const AdjacencyList<Index>& adjacency_list) {
        mesh.initialize_edges();
        for (Index ei = 0; ei < mesh.get_num_edges(); ei++) {
            const auto e_vertices = mesh.get_edge_vertices(ei);
            const Index v0 = e_vertices[0];
            const Index v1 = e_vertices[1];

            const auto v0_adj = adjacency_list.get_neighbors(v0);
            const auto v1_adj = adjacency_list.get_neighbors(v1);

            REQUIRE(std::find(v0_adj.begin(), v0_adj.end(), v1) != v0_adj.end());
            REQUIRE(std::find(v1_adj.begin(), v1_adj.end(), v0) != v1_adj.end());
        }
    };

    SECTION("single triangle")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);

        auto adjacency_list = compute_vertex_vertex_adjacency(mesh);
        REQUIRE(adjacency_list.get_num_entries() == 3);
        check_adjacency(mesh, adjacency_list);
    }

    SECTION("single quad")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_quad(0, 1, 3, 2);

        auto adjacency_list = compute_vertex_vertex_adjacency(mesh);
        REQUIRE(adjacency_list.get_num_entries() == 4);
        check_adjacency(mesh, adjacency_list);
    }

    SECTION("2 triangles")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(2, 1, 3);

        auto adjacency_list = compute_vertex_vertex_adjacency(mesh);
        check_adjacency(mesh, adjacency_list);
    }

    SECTION("quad + tri")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_vertex({2, 0, 0});
        mesh.add_quad(0, 1, 3, 2);
        mesh.add_triangle(3, 1, 4);

        auto adjacency_list = compute_vertex_vertex_adjacency(mesh);
        check_adjacency(mesh, adjacency_list);
    }
}

TEST_CASE(
    "compute_vertex_vertex_adjacency benchmark",
    "[surface][adjacency][utilities][!benchmark]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");
    BENCHMARK("compute_vertex_vertex_adjacency")
    {
        return compute_vertex_vertex_adjacency(mesh);
    };

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    using MeshType = TriangleMesh3D;
    BENCHMARK_ADVANCED("Mesh::initialize_connectivity")(Catch::Benchmark::Chronometer meter)
    {
        auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);
        meter.measure([&]() {
            legacy_mesh->initialize_connectivity();
            return legacy_mesh->get_vertex_vertex_adjacency();
        });
    };
#endif
}

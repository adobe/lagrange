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
#include <catch2/benchmark/catch_benchmark.hpp>

#include <lagrange/common.h>
#include <lagrange/compute_components.h>
#include <lagrange/create_mesh.h>
#include <lagrange/mesh_cleanup/resolve_vertex_nonmanifoldness.h>
#include <lagrange/mesh_convert.h>
#include <lagrange/testing/check_mesh.h>
#include <lagrange/topology.h>
#include <lagrange/views.h>


#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
TEST_CASE("legacy::resolve_vertex_nonmanifoldness", "[nonmanifold][Mesh][cleanup]")
{
    using namespace lagrange;

    SECTION("simple")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 3> vertices(3, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;
        Eigen::Matrix<int, Eigen::Dynamic, 3> facets(1, 3);
        facets << 0, 1, 2;

        auto in_mesh = lagrange::create_mesh(vertices, facets);
        in_mesh->initialize_topology();
        REQUIRE(in_mesh->is_vertex_manifold());

        in_mesh->initialize_connectivity();

        auto out_mesh = lagrange::resolve_vertex_nonmanifoldness(*in_mesh);
        out_mesh->initialize_topology();
        REQUIRE(out_mesh->is_vertex_manifold());
    }

    SECTION("two triangles")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 3> vertices(4, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0;
        Eigen::Matrix<int, Eigen::Dynamic, 3> facets(2, 3);
        facets << 0, 1, 2, 1, 0, 3;

        auto in_mesh = lagrange::create_mesh(vertices, facets);
        in_mesh->initialize_topology();
        REQUIRE(in_mesh->is_vertex_manifold());

        in_mesh->initialize_connectivity();

        auto out_mesh = lagrange::resolve_vertex_nonmanifoldness(*in_mesh);
        out_mesh->initialize_topology();
        REQUIRE(out_mesh->is_vertex_manifold());
    }
}
#endif

TEST_CASE("resolve_vertex_nonmanifoldness", "[nonmanifold][surface][cleanup]")
{
    using namespace lagrange;

    using Scalar = double;
    using Index = uint32_t;
    lagrange::SurfaceMesh<Scalar, Index> mesh;

    SECTION("simple")
    {
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);

        lagrange::testing::check_mesh(mesh);
        REQUIRE(is_vertex_manifold(mesh));

        resolve_vertex_nonmanifoldness(mesh);

        lagrange::testing::check_mesh(mesh);
        REQUIRE(is_vertex_manifold(mesh));
        REQUIRE(mesh.get_num_vertices() == 3);
        REQUIRE(mesh.get_num_facets() == 1);
    }

    SECTION("two triangles")
    {
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({0, 0, 1});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 0, 3);

        lagrange::testing::check_mesh(mesh);
        REQUIRE(is_vertex_manifold(mesh));

        resolve_vertex_nonmanifoldness(mesh);

        lagrange::testing::check_mesh(mesh);
        REQUIRE(is_vertex_manifold(mesh));
    }

    SECTION("two triangles touching at a vertex")
    {
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({-1, 0, 0});
        mesh.add_vertex({0, -1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 3, 4);

        lagrange::testing::check_mesh(mesh);
        REQUIRE(!is_vertex_manifold(mesh));

        resolve_vertex_nonmanifoldness(mesh);

        lagrange::testing::check_mesh(mesh);
        REQUIRE(is_vertex_manifold(mesh));
    }

    SECTION("two tets touching at a vertex")
    {
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({0, 0, 1});
        mesh.add_vertex({-1, 0, 0});
        mesh.add_vertex({0, -1, 0});
        mesh.add_vertex({0, 0, -1});

        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 2, 3);
        mesh.add_triangle(0, 3, 1);
        mesh.add_triangle(3, 2, 1);
        mesh.add_triangle(0, 4, 5);
        mesh.add_triangle(0, 5, 6);
        mesh.add_triangle(0, 6, 4);
        mesh.add_triangle(6, 5, 4);

        ComponentOptions opt;
        opt.connectivity_type = ConnectivityType::Vertex;

        lagrange::testing::check_mesh(mesh);
        REQUIRE(!is_vertex_manifold(mesh));
        REQUIRE(compute_components(mesh, opt) == 1);

        resolve_vertex_nonmanifoldness(mesh);

        lagrange::testing::check_mesh(mesh);
        REQUIRE(is_vertex_manifold(mesh));
        REQUIRE(compute_components(mesh, opt) == 2);
    }

    SECTION("Facet consists of 2 vertices")
    {
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_polygon({0, 1});
        mesh.add_polygon({1, 2});

        ComponentOptions opt;
        opt.connectivity_type = ConnectivityType::Vertex;

        lagrange::testing::check_mesh(mesh);
        REQUIRE(compute_components(mesh, opt) == 1);

        resolve_vertex_nonmanifoldness(mesh);

        lagrange::testing::check_mesh(mesh);
        REQUIRE(compute_components(mesh, opt) == 2);
    }

    SECTION("hemisphere")
    {
        mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/hemisphere.obj");
        REQUIRE(is_vertex_manifold(mesh));
        lagrange::testing::check_mesh(mesh);
        auto num_vertices = mesh.get_num_vertices();
        auto num_facets = mesh.get_num_facets();
        resolve_vertex_nonmanifoldness(mesh);
        REQUIRE(is_vertex_manifold(mesh));
        REQUIRE(mesh.get_num_vertices() == num_vertices);
        REQUIRE(mesh.get_num_facets() == num_facets);
        lagrange::testing::check_mesh(mesh);
    }
}

TEST_CASE(
    "resolve_vertex_nonmanifoldness benchmark",
    "[surface][mesh_cleanup][nonmanifold][!benchmark]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");

    // Drop facets with even index. The resulting mesh will have non-manifold vertices.
    // In this particular example, there is a total of 141406 non-manifold vertices.
    const Index num_facets = mesh.get_num_facets();
    std::vector<Index> even_facets(num_facets / 2);
    for (Index i = 0; i < num_facets; i += 2) {
        even_facets[i / 2] = i;
    }
    mesh.remove_facets(even_facets);
    REQUIRE(!is_vertex_manifold(mesh));
    REQUIRE(is_edge_manifold(mesh));

    auto id = compute_vertex_is_manifold(mesh);
    auto vertex_manifold = attribute_vector_view<uint8_t>(mesh, id).template cast<Index>();
    REQUIRE(vertex_manifold.sum() == 141406);


    BENCHMARK_ADVANCED("resolve_vertex_nonmanifoldness")
    (Catch::Benchmark::Chronometer meter)
    {
        auto mesh_copy = mesh;
        meter.measure([&]() {
            resolve_vertex_nonmanifoldness(mesh_copy);
            return mesh_copy;
        });
    };
}

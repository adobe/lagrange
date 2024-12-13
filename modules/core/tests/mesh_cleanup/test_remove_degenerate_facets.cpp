/*
 * Copyright 2024 Adobe. All rights reserved.
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

#include <lagrange/mesh_cleanup/detect_degenerate_facets.h>
#include <lagrange/mesh_cleanup/remove_degenerate_facets.h>
#include <lagrange/mesh_convert.h>

TEST_CASE("remove_degenerate_facets", "[surface_mesh][mesh_cleanup]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SECTION("single triangle - no degeneracy")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);

        remove_degenerate_facets(mesh);
        REQUIRE(mesh.get_num_facets() == 1);
        REQUIRE(mesh.get_num_vertices() == 3);

        auto degenerate_facets = detect_degenerate_facets(mesh);
        REQUIRE(degenerate_facets.empty());
    }

    SECTION("single triangle - with degeneracy")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0.5, 0, 0});
        mesh.add_triangle(0, 1, 2);

        remove_degenerate_facets(mesh);
        REQUIRE(mesh.get_num_facets() == 0);
        REQUIRE(mesh.get_num_vertices() == 3);

        auto degenerate_facets = detect_degenerate_facets(mesh);
        REQUIRE(degenerate_facets.empty());
    }

    SECTION("two triangles")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({0.5, 0.5, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(2, 1, 3);

        remove_degenerate_facets(mesh);
        REQUIRE(mesh.get_num_facets() == 2);
        REQUIRE(mesh.get_num_vertices() == 4);

        auto degenerate_facets = detect_degenerate_facets(mesh);
        REQUIRE(degenerate_facets.empty());
    }

    SECTION("three triangles")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({0.5, 0.5, 0});
        mesh.add_vertex({0.5, 0, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(2, 1, 3);
        mesh.add_triangle(0, 4, 1);

        remove_degenerate_facets(mesh);
        REQUIRE(mesh.get_num_facets() == 3);
        REQUIRE(mesh.get_num_vertices() == 5);

        auto degenerate_facets = detect_degenerate_facets(mesh);
        REQUIRE(degenerate_facets.empty());
    }

    SECTION("three triangles v2")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({0.5, 0.5, 0});
        mesh.add_vertex({0.2, 0.8, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(2, 1, 3);
        mesh.add_triangle(2, 3, 4);

        remove_degenerate_facets(mesh);
        REQUIRE(mesh.get_num_facets() == 3);
        REQUIRE(mesh.get_num_vertices() == 5);

        auto degenerate_facets = detect_degenerate_facets(mesh);
        REQUIRE(degenerate_facets.empty());
    }

    SECTION("three triangles with duplicate vertex")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({0.5, 0.5, 0});
        mesh.add_vertex({0.5, 0.5, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(2, 1, 3);
        mesh.add_triangle(2, 3, 4);

        remove_degenerate_facets(mesh);
        REQUIRE(mesh.get_num_facets() == 2);
        REQUIRE(mesh.get_num_vertices() == 4);

        auto degenerate_facets = detect_degenerate_facets(mesh);
        REQUIRE(degenerate_facets.empty());
    }

    SECTION("Nonmanifold T junction")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_vertex({0.5, 0.5, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(2, 1, 3);
        mesh.add_triangle(1, 2, 4);

        remove_degenerate_facets(mesh);
        REQUIRE(mesh.get_num_facets() == 4);
        REQUIRE(mesh.get_num_vertices() == 5);

        auto degenerate_facets = detect_degenerate_facets(mesh);
        REQUIRE(degenerate_facets.empty());
    }

    SECTION("three collinear triangles")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({01, 0, 0});
        mesh.add_vertex({0.2, 0, 0});
        mesh.add_vertex({0.3, 0, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 1, 3);
        mesh.add_triangle(0, 1, 4);

        remove_degenerate_facets(mesh);
        REQUIRE(mesh.get_num_facets() == 0);
        REQUIRE(mesh.get_num_vertices() == 4);

        auto degenerate_facets = detect_degenerate_facets(mesh);
        REQUIRE(degenerate_facets.empty());
    }

    SECTION("stacked degeneracy")
    {
        const Index N = 10;
        SurfaceMesh<Scalar, Index> mesh;
        for (Index i = 0; i < N; i++) {
            mesh.add_vertex({static_cast<Scalar>(i), 0, 0});
        }
        mesh.add_vertex({0, 1, 0});

        for (Index i = 1; i < N - 1; i++) {
            mesh.add_triangle(0, i, N - 1);
        }
        mesh.add_triangle(0, N, N - 1);

        std::vector<int> facet_index;
        for (Index i = 0; i < N - 1; i++) {
            facet_index.push_back(static_cast<int>(i));
        }
        mesh.template create_attribute<int>(
            "facet_index",
            AttributeElement::Facet,
            1,
            AttributeUsage::Scalar,
            {facet_index.data(), facet_index.size()});

        remove_degenerate_facets(mesh);
        REQUIRE(mesh.get_num_facets() == N - 1);

        REQUIRE(mesh.has_attribute("facet_index"));
        auto f_indices = mesh.template get_attribute<int>("facet_index").get_all();
        for (auto f_idx : f_indices) {
            REQUIRE(f_idx == N - 2);
        }

        auto degenerate_facets = detect_degenerate_facets(mesh);
        REQUIRE(degenerate_facets.empty());
    }
}

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/Mesh.h>
    #include <lagrange/mesh_cleanup/remove_degenerate_triangles.h>
#endif
TEST_CASE("remove_degenerate_facets benchmark", "[surface][clenaup][!benchmark]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");
    BENCHMARK_ADVANCED("remove_degenerate_facets")(Catch::Benchmark::Chronometer meter)
    {
        auto mesh_copy = SurfaceMesh<Scalar, Index>::stripped_copy(mesh);
        meter.measure([&] { remove_degenerate_facets(mesh_copy); });
        REQUIRE(mesh_copy.get_num_facets() != mesh.get_num_facets());
        return mesh_copy;
    };

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    using MeshType = TriangleMesh3D;
    auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);
    BENCHMARK("legacy::remove_degenerate_triangles")
    {
        auto mesh2 = legacy::remove_degenerate_triangles(*legacy_mesh);
        REQUIRE(mesh2->get_num_facets() != legacy_mesh->get_num_facets());
        return mesh2;
    };
#endif
}

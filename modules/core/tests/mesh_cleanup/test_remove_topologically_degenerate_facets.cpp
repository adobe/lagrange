/*
 * Copyright 2023 Adobe. All rights reserved.
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

#include <lagrange/mesh_cleanup/remove_topologically_degenerate_facets.h>
#include <lagrange/mesh_convert.h>
#include <lagrange/testing/check_mesh.h>

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/mesh_cleanup/legacy/remove_topologically_degenerate_triangles.h>
#endif

TEST_CASE("remove_topologically_degenerate_facets", "[mesh_cleanup][surface][degenerate]")
{
    using namespace lagrange;
    using Scalar = float;
    using Index = uint32_t;

    SECTION("empty mesh")
    {
        SurfaceMesh<Scalar, Index> mesh;
        remove_topologically_degenerate_facets(mesh);
        REQUIRE(mesh.get_num_vertices() == 0);
        REQUIRE(mesh.get_num_facets() == 0);
        lagrange::testing::check_mesh(mesh);
    }

    SECTION("non-degenerate triangle")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);
        remove_topologically_degenerate_facets(mesh);
        REQUIRE(mesh.get_num_vertices() == 3);
        REQUIRE(mesh.get_num_facets() == 1);
        lagrange::testing::check_mesh(mesh);
    }

    SECTION("degenerate triangle")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_triangle(0, 1, 1);
        remove_topologically_degenerate_facets(mesh);
        REQUIRE(mesh.get_num_vertices() == 2);
        REQUIRE(mesh.get_num_facets() == 0);
        lagrange::testing::check_mesh(mesh);
    }

    SECTION("two degenerate triangles")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 1);
        mesh.add_triangle(1, 1, 2);
        remove_topologically_degenerate_facets(mesh);
        REQUIRE(mesh.get_num_vertices() == 3);
        REQUIRE(mesh.get_num_facets() == 0);
        lagrange::testing::check_mesh(mesh);
    }

    SECTION("with facet attribute")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 2, 3);
        mesh.add_triangle(0, 0, 1);
        mesh.create_attribute<uint8_t>(
            "facet_index",
            AttributeElement::Facet,
            AttributeUsage::Scalar,
            1,
            {{1, 2, 3}});
        remove_topologically_degenerate_facets(mesh);
        REQUIRE(mesh.get_num_vertices() == 4);
        REQUIRE(mesh.get_num_facets() == 2);
        lagrange::testing::check_mesh(mesh);

        REQUIRE(mesh.has_attribute("facet_index"));
        const auto& attr = mesh.get_attribute<uint8_t>("facet_index");
        REQUIRE(attr.get(0) == 1);
        REQUIRE(attr.get(1) == 2);
    }
}

TEST_CASE(
    "remove_topologically_degenerate_facets benchmark",
    "[surface][mesh_cleanup][degenerate][!benchmark]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");

    BENCHMARK_ADVANCED("remove_topologically_degenerate_facets")
    (Catch::Benchmark::Chronometer meter)
    {
        auto mesh_copy = mesh;
        meter.measure([&]() { return remove_topologically_degenerate_facets(mesh_copy); });
    };

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    using MeshType = TriangleMesh3D;
    auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);

    BENCHMARK_ADVANCED("legacy::remove_topologically_degenerate_triangles")
    (Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&]() { return remove_topologically_degenerate_triangles(*legacy_mesh); });
    };
#endif
}

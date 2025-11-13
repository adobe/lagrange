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

#include <lagrange/Attribute.h>
#include <lagrange/compute_vertex_valence.h>

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/Mesh.h>
    #include <lagrange/mesh_convert.h>
#endif

TEST_CASE("compute_vertex_valence", "[surface][attribute][utilities]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    VertexValenceOptions opt;

    SECTION("single triangle")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);

        auto id = compute_vertex_valence(mesh, opt);
        REQUIRE(mesh.is_attribute_type<Index>(id));
        const auto& valence_attr = mesh.template get_attribute<Index>(id);

        REQUIRE(valence_attr.get_num_elements() == mesh.get_num_vertices());
        REQUIRE(valence_attr.get(0) == 2);
        REQUIRE(valence_attr.get(1) == 2);
        REQUIRE(valence_attr.get(2) == 2);
    }

    SECTION("two traingles")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(2, 1, 3);

        auto id = compute_vertex_valence(mesh, opt);
        const auto& valence_attr = mesh.template get_attribute<Index>(id);

        REQUIRE(valence_attr.get_num_elements() == mesh.get_num_vertices());
        REQUIRE(valence_attr.get(0) == 2);
        REQUIRE(valence_attr.get(1) == 3);
        REQUIRE(valence_attr.get(2) == 3);
        REQUIRE(valence_attr.get(3) == 2);
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

        auto id = compute_vertex_valence(mesh, opt);
        const auto& valence_attr = mesh.template get_attribute<Index>(id);

        REQUIRE(valence_attr.get_num_elements() == mesh.get_num_vertices());
        REQUIRE(valence_attr.get(0) == 2);
        REQUIRE(valence_attr.get(1) == 3);
        REQUIRE(valence_attr.get(2) == 2);
        REQUIRE(valence_attr.get(3) == 3);
        REQUIRE(valence_attr.get(4) == 2);
    }
}

TEST_CASE("compute_vertex_valence benchmark", "[surface][attribute][utilities][!benchmark]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");
    BENCHMARK("compute_vertex_valence")
    {
        return compute_vertex_valence(mesh);
    };

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    using MeshType = TriangleMesh3D;
    BENCHMARK_ADVANCED("compute_vertex_valence legacy")(Catch::Benchmark::Chronometer meter)
    {
        auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);
        meter.measure([&]() {
            compute_vertex_valence(*legacy_mesh);
            return legacy_mesh->get_vertex_attribute("valence");
        });
    };
#endif
}

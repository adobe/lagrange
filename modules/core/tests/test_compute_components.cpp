/*
 * Copyright 2017 Adobe. All rights reserved.
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

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <Eigen/Core>

    #include <lagrange/Components.h>
    #include <lagrange/Connectivity.h>
    #include <lagrange/Mesh.h>
    #include <lagrange/common.h>
    #include <lagrange/create_mesh.h>
#endif

#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/compute_components.h>
#include <lagrange/mesh_convert.h>
#include <lagrange/views.h>

TEST_CASE("compute_components", "[surface][components][utilities]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SECTION("empty mesh")
    {
        SurfaceMesh<Scalar, Index> mesh;
        auto num_components = compute_components(mesh);
        REQUIRE(num_components == 0);
    }

    ComponentOptions opt;

    SECTION("single component")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);
        auto num_components = compute_components(mesh, opt);
        REQUIRE(num_components == 1);

        const auto& attr_id = mesh.get_attribute<Index>(opt.output_attribute_name);
        REQUIRE(attr_id.get_num_elements() == 1);
        REQUIRE(attr_id.get_num_channels() == 1);
        REQUIRE(attr_id.get(0) == 0);
    }

    SECTION("two components")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({0, 0, 1});
        mesh.add_vertex({1, 0, 1});
        mesh.add_vertex({0, 1, 1});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(3, 4, 5);
        auto num_components = compute_components(mesh, opt);
        REQUIRE(num_components == 2);

        const auto& attr_id = mesh.get_attribute<Index>(opt.output_attribute_name);
        REQUIRE(attr_id.get_num_elements() == 2);
        REQUIRE(attr_id.get_num_channels() == 1);
        REQUIRE(attr_id.get(0) != attr_id.get(1));
    }

    SECTION("two triangles touching at a vertex")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({-1, 0, 0});
        mesh.add_vertex({0, -1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 3, 4);

        SECTION("with vertex connectivity")
        {
            opt.connectivity_type = ComponentOptions::ConnectivityType::Vertex;
            auto num_components = compute_components(mesh, opt);
            REQUIRE(num_components == 1);
        }
        SECTION("with edge connectivity")
        {
            opt.connectivity_type = ComponentOptions::ConnectivityType::Edge;
            auto num_components = compute_components(mesh, opt);
            REQUIRE(num_components == 2);
        }
    }

    SECTION("two triangles touching at an edge")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(2, 1, 3);

        SECTION("with vertex connectivity")
        {
            opt.connectivity_type = ComponentOptions::ConnectivityType::Vertex;
            auto num_components = compute_components(mesh, opt);
            REQUIRE(num_components == 1);
        }
        SECTION("with edge connectivity")
        {
            opt.connectivity_type = ComponentOptions::ConnectivityType::Edge;
            auto num_components = compute_components(mesh, opt);
            REQUIRE(num_components == 1);
        }

        const auto& attr_id = mesh.get_attribute<Index>(opt.output_attribute_name);
        REQUIRE(attr_id.get_num_elements() == 2);
        REQUIRE(attr_id.get_num_channels() == 1);
        REQUIRE(attr_id.get(0) == 0);
        REQUIRE(attr_id.get(1) == 0);
    }
}

TEST_CASE("compute_components benchmark", "[surface][components][utilities][!benchmark]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");
    const auto& vertex_attr = mesh.get_vertex_to_position();
    const auto& facet_attr = mesh.get_corner_to_vertex();

    auto wrap_copy = [&]() {
        SurfaceMesh<Scalar, Index> tmp_mesh;
        tmp_mesh.wrap_as_const_vertices(vertex_attr.get_all(), vertex_attr.get_num_elements());
        tmp_mesh.wrap_as_const_facets(
            facet_attr.get_all(),
            mesh.get_num_facets(),
            mesh.get_vertex_per_facet());
        return tmp_mesh;
    };

    SECTION("Without initial computation")
    {
        BENCHMARK_ADVANCED("compute_components (disjoint sets)")
        (Catch::Benchmark::Chronometer meter)
        {
            auto tmp_mesh = wrap_copy();
            tmp_mesh.initialize_edges();
            ComponentOptions opt;
            meter.measure([&]() { return compute_components(tmp_mesh, opt); });
        };

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
        using MeshType = TriangleMesh3D;
        auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);
        BENCHMARK_ADVANCED("compute_components legacy")(Catch::Benchmark::Chronometer meter)
        {
            auto tmp_mesh = wrap_with_mesh(legacy_mesh->get_vertices(), legacy_mesh->get_facets());
            tmp_mesh->initialize_connectivity();
            meter.measure([&]() {
                return tmp_mesh->initialize_components();
            });
        };
#endif
    }

    SECTION("With initial computation")
    {
        BENCHMARK_ADVANCED("compute_components (disjoint sets)")
        (Catch::Benchmark::Chronometer meter)
        {
            auto tmp_mesh = wrap_copy();
            ComponentOptions opt;
            meter.measure([&]() { return compute_components(tmp_mesh, opt); });
        };

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
        using MeshType = TriangleMesh3D;
        auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);
        BENCHMARK_ADVANCED("compute_components legacy")(Catch::Benchmark::Chronometer meter)
        {
            auto tmp_mesh = wrap_with_mesh(legacy_mesh->get_vertices(), legacy_mesh->get_facets());
            meter.measure([&]() {
                tmp_mesh->initialize_connectivity();
                return tmp_mesh->initialize_components();
            });
        };
#endif
    }
}

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
TEST_CASE("Components", "[components][simple]")
{
    using namespace lagrange;

    Vertices3D vertices(3, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;
    Triangles facets(1, 3);
    facets << 0, 1, 2;

    auto mesh = create_mesh(vertices, facets);
    mesh->initialize_components();

    REQUIRE(mesh->get_num_components() == 1);
    const auto& comp_list = mesh->get_components();
    REQUIRE(comp_list[0].size() == 1);
}

TEST_CASE("ComponentsVertexTouch", "[components][simple]")
{
    using namespace lagrange;

    Vertices3D vertices(5, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0;
    Triangles facets(2, 3);
    facets << 0, 1, 2, 0, 3, 4;

    auto mesh = create_mesh(vertices, facets);
    mesh->initialize_components();

    REQUIRE(mesh->get_num_components() == 2);
    const auto& comp_list = mesh->get_components();
    REQUIRE(comp_list[0].size() == 1);
    REQUIRE(comp_list[1].size() == 1);

    const auto& comp_ids = mesh->get_per_facet_component_ids();
    REQUIRE(comp_ids.size() == 2);
}

TEST_CASE("MultiComps", "[components][simple]")
{
    using namespace lagrange;

    Vertices3D vertices(6, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0,
        1.0;
    Triangles facets(2, 3);
    facets << 0, 1, 2, 3, 4, 5;

    auto mesh = create_mesh(vertices, facets);
    mesh->initialize_components();

    REQUIRE(mesh->get_num_components() == 2);
    const auto& comp_list = mesh->get_components();
    REQUIRE(comp_list[0].size() == 1);
    REQUIRE(comp_list[1].size() == 1);

    const auto& comp_ids = mesh->get_per_facet_component_ids();
    REQUIRE(comp_ids.size() == 2);
}
#endif

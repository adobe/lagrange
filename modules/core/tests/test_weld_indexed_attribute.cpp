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
#include <lagrange/IndexedAttribute.h>
#include <lagrange/testing/common.h>
#include <lagrange/weld_indexed_attribute.h>

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/attributes/condense_indexed_attribute.h>
    #include <lagrange/mesh_convert.h>
#endif

#include <catch2/benchmark/catch_benchmark.hpp>

TEST_CASE("weld_indexed_attribute", "[core][attribute][surface]")
{
    using namespace lagrange;
    using Scalar = float;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh(2);
    mesh.add_vertex({0, 0});
    mesh.add_vertex({1, 0});
    mesh.add_vertex({0, 1});
    mesh.add_vertex({1, 1});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(2, 1, 3);

    SECTION("Distinct attributes")
    {
        std::array<Scalar, 12> uv_values{0, 0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0};
        std::array<Index, 6> uv_indices{0, 1, 2, 3, 4, 5};
        auto id = mesh.create_attribute<Scalar>(
            "uv",
            AttributeElement::Indexed,
            AttributeUsage::UV,
            2,
            uv_values,
            uv_indices);
        weld_indexed_attribute(mesh, id);

        auto& attr = mesh.get_indexed_attribute<Scalar>(id);
        auto& values = attr.values();

        REQUIRE(values.get_num_elements() == 6);
        for (Index i = 0; i < 6; i++) {
            REQUIRE(values.get(i, 0) == i);
            REQUIRE(values.get(i, 1) == 0);
        }
    }

    SECTION("With duplicate attributes")
    {
        std::array<Scalar, 12> uv_values{0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 1};
        std::array<Index, 6> uv_indices{0, 1, 2, 3, 4, 5};
        auto id = mesh.create_attribute<Scalar>(
            "uv",
            AttributeElement::Indexed,
            AttributeUsage::UV,
            2,
            uv_values,
            uv_indices);
        weld_indexed_attribute(mesh, id);

        auto& attr = mesh.get_indexed_attribute<Scalar>(id);
        auto& values = attr.values();
        auto& indices = attr.indices();

        REQUIRE(values.get_num_elements() == 4);

        for (Index ci = 0; ci < mesh.get_num_corners(); ci++) {
            Index vi = mesh.get_corner_vertex(ci);
            auto v = mesh.get_position(vi);
            Index i = indices.get(ci);
            REQUIRE(values.get(i, 0) == v[0]);
            REQUIRE(values.get(i, 1) == v[1]);
        }
    }
}

TEST_CASE("weld_indexed_attribute benchmark", "[surface][attribute][utilities][!benchmark]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");
    const Index num_corners = mesh.get_num_corners();
    std::vector<Scalar> x_coordinate(num_corners);
    for (Index ci = 0; ci < num_corners; ci++) {
        Index vi = mesh.get_corner_vertex(ci);
        x_coordinate[ci] = mesh.get_position(vi)[0];
    }
    std::vector<Index> x_indices(num_corners);
    std::iota(x_indices.begin(), x_indices.end(), 0);

    BENCHMARK_ADVANCED("weld_indexed_buffer")(Catch::Benchmark::Chronometer meter)
    {
        auto id = mesh.template create_attribute<Scalar>(
            "x",
            AttributeElement::Indexed,
            AttributeUsage::Scalar,
            1,
            {x_coordinate.data(), x_coordinate.size()},
            {x_indices.data(), x_indices.size()});
        meter.measure([&]() { weld_indexed_attribute(mesh, id); });
        auto& attr = mesh.get_indexed_attribute<Scalar>(id);
        REQUIRE(attr.values().get_num_elements() == mesh.get_num_vertices());
        mesh.delete_attribute("x");
    };

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    using MeshType = TriangleMesh3D;
    mesh.template create_attribute<Scalar>(
        "x",
        AttributeElement::Indexed,
        AttributeUsage::Scalar,
        1,
        {x_coordinate.data(), x_coordinate.size()},
        {x_indices.data(), x_indices.size()});
    auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);

    BENCHMARK_ADVANCED("legacy::condense_indexed_attribute")(Catch::Benchmark::Chronometer meter)
    {
        meter.measure([&]() { return condense_indexed_attribute(*legacy_mesh, "x", "x2"); });
        const auto& [values, indices] = legacy_mesh->get_indexed_attribute("x2");
        REQUIRE(static_cast<MeshType::Index>(values.rows()) == legacy_mesh->get_num_vertices());
        legacy_mesh->remove_indexed_attribute("x2");
    };
#endif
}

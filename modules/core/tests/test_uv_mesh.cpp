/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/Attribute.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/testing/common.h>
#include <lagrange/uv_mesh.h>
#include <lagrange/views.h>

TEST_CASE("uv_mesh: indexed attribute", "[core][uv_mesh]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    // Create a mesh with 2 triangles sharing an edge
    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(2, 1, 3);

    // Add indexed UV attribute
    std::vector<Scalar> uv_values{0, 0, 1, 0, 0, 1, 1, 1};
    std::vector<Index> uv_indices{0, 1, 2, 2, 1, 3};
    mesh.template create_attribute<Scalar>(
        "uv",
        AttributeElement::Indexed,
        AttributeUsage::UV,
        2,
        uv_values,
        uv_indices);

    SECTION("uv_mesh_view")
    {
        auto uv = uv_mesh_view(mesh);
        REQUIRE(uv.get_num_vertices() == 4);
        REQUIRE(uv.get_num_facets() == 2);
        REQUIRE(uv.get_num_corners() == 6);
    }

    SECTION("uv_mesh_ref")
    {
        auto uv = uv_mesh_ref(mesh);
        REQUIRE(uv.get_num_vertices() == 4);
        REQUIRE(uv.get_num_facets() == 2);
        REQUIRE(uv.get_num_corners() == 6);
    }
}

TEST_CASE("uv_mesh: vertex attribute", "[core][uv_mesh]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(2, 1, 3);

    // Add vertex UV attribute (UV == position xy)
    std::vector<Scalar> uv_values{0, 0, 1, 0, 0, 1, 1, 1};
    mesh.template create_attribute<Scalar>(
        "uv",
        AttributeElement::Vertex,
        AttributeUsage::UV,
        2,
        uv_values);

    SECTION("uv_mesh_view")
    {
        auto uv = uv_mesh_view(mesh);
        REQUIRE(uv.get_num_vertices() == 4);
        REQUIRE(uv.get_num_facets() == 2);
        REQUIRE(uv.get_num_corners() == 6);
    }

    SECTION("uv_mesh_ref")
    {
        auto uv = uv_mesh_ref(mesh);
        REQUIRE(uv.get_num_vertices() == 4);
        REQUIRE(uv.get_num_facets() == 2);
        REQUIRE(uv.get_num_corners() == 6);
    }
}

TEST_CASE("uv_mesh: corner attribute", "[core][uv_mesh]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(2, 1, 3);

    // Add corner UV attribute (one UV per corner = 6 values)
    std::vector<Scalar> uv_values{0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 1};
    mesh.template create_attribute<Scalar>(
        "uv",
        AttributeElement::Corner,
        AttributeUsage::UV,
        2,
        uv_values);

    SECTION("default element_types rejects corner attribute")
    {
        UVMeshOptions options;
        options.uv_attribute_name = "uv";
        LA_REQUIRE_THROWS(uv_mesh_view(mesh, options));
    }

    SECTION("uv_mesh_view with UVMeshOptions::ElementTypes::All")
    {
        UVMeshOptions options;
        options.uv_attribute_name = "uv";
        options.element_types = UVMeshOptions::ElementTypes::All;
        auto uv = uv_mesh_view(mesh, options);
        REQUIRE(uv.get_num_vertices() == 6);
        REQUIRE(uv.get_num_facets() == 2);
        REQUIRE(uv.get_num_corners() == 6);
        REQUIRE(uv.get_vertex_per_facet() == 3);

        // Verify vertex positions match original corner UV values
        for (Index c = 0; c < 6; ++c) {
            Index v = uv.get_corner_vertex(c);
            REQUIRE(v == c);
            auto p = uv.get_position(v);
            REQUIRE(p[0] == uv_values[c * 2]);
            REQUIRE(p[1] == uv_values[c * 2 + 1]);
        }
    }

    SECTION("uv_mesh_ref with UVMeshOptions::ElementTypes::All")
    {
        UVMeshOptions options;
        options.uv_attribute_name = "uv";
        options.element_types = UVMeshOptions::ElementTypes::All;
        auto uv = uv_mesh_ref(mesh, options);
        REQUIRE(uv.get_num_vertices() == 6);
        REQUIRE(uv.get_num_facets() == 2);
        REQUIRE(uv.get_num_corners() == 6);

        // Modify UV mesh vertex and verify it reflects in the original corner attribute
        auto uv_pos = matrix_ref(uv.ref_vertex_to_position());
        uv_pos(0, 0) = 42.0;
        uv_pos(0, 1) = 43.0;

        auto& corner_attr = mesh.get_attribute<Scalar>("uv");
        auto corner_data = corner_attr.get_all();
        REQUIRE(corner_data[0] == 42.0);
        REQUIRE(corner_data[1] == 43.0);
    }
}

TEST_CASE("uv_mesh: corner attribute hybrid mesh", "[core][uv_mesh]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    // Create a hybrid mesh with 1 triangle + 1 quad
    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_vertex({2, 0, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_quad(1, 4, 3, 2);

    // 3 + 4 = 7 corners
    std::vector<Scalar> uv_values{0, 0, 1, 0, 0, 1, 1, 0, 2, 0, 1, 1, 0, 1};
    mesh.template create_attribute<Scalar>(
        "uv",
        AttributeElement::Corner,
        AttributeUsage::UV,
        2,
        uv_values);

    UVMeshOptions options;
    options.uv_attribute_name = "uv";
    options.element_types = UVMeshOptions::ElementTypes::All;

    SECTION("uv_mesh_view")
    {
        auto uv = uv_mesh_view(mesh, options);
        REQUIRE(uv.get_num_vertices() == 7);
        REQUIRE(uv.get_num_facets() == 2);
        REQUIRE(uv.get_num_corners() == 7);
        REQUIRE(uv.get_facet_size(0) == 3);
        REQUIRE(uv.get_facet_size(1) == 4);

        for (Index c = 0; c < 7; ++c) {
            Index v = uv.get_corner_vertex(c);
            REQUIRE(v == c);
        }
    }

    SECTION("uv_mesh_ref")
    {
        auto uv = uv_mesh_ref(mesh, options);
        REQUIRE(uv.get_num_vertices() == 7);
        REQUIRE(uv.get_num_facets() == 2);
        REQUIRE(uv.get_num_corners() == 7);
        REQUIRE(uv.get_facet_size(0) == 3);
        REQUIRE(uv.get_facet_size(1) == 4);
    }
}

TEST_CASE(
    "uv_mesh: auto-detect corner attribute with UVMeshOptions::ElementTypes::All",
    "[core][uv_mesh]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);

    // Only a corner UV attribute (no name specified, auto-detect)
    std::vector<Scalar> uv_values{0, 0, 1, 0, 0, 1};
    mesh.template create_attribute<Scalar>(
        "uv",
        AttributeElement::Corner,
        AttributeUsage::UV,
        2,
        uv_values);

    UVMeshOptions options;
    options.element_types = UVMeshOptions::ElementTypes::All;
    auto uv = uv_mesh_view(mesh, options);
    REQUIRE(uv.get_num_vertices() == 3);
    REQUIRE(uv.get_num_facets() == 1);
    REQUIRE(uv.get_num_corners() == 3);
}

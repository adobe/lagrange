/*
 * Copyright 2020 Adobe. All rights reserved.
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
#include <lagrange/find_matching_attributes.h>
#include <lagrange/internal/attribute_string_utils.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/map_attribute.h>
#include <lagrange/orient_outward.h>
#include <lagrange/testing/check_mesh.h>
#include <lagrange/utils/fmt_eigen.h>
#include <lagrange/views.h>
#include <lagrange/weld_indexed_attribute.h>

TEST_CASE("orient_outward", "[mesh][orient]")
{
    using Scalar = double;
    using Index = uint32_t;

    auto mesh_in = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/torus3_in.obj");
    auto mesh_out = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/torus3_out.obj");

    SECTION("without edges")
    {
        lagrange::orient_outward(mesh_in);
        REQUIRE(vertex_view(mesh_in) == vertex_view(mesh_out));
        REQUIRE(facet_view(mesh_in) == facet_view(mesh_out));
        lagrange::testing::check_mesh(mesh_in);
    }

    SECTION("with edges")
    {
        mesh_in.initialize_edges();
        lagrange::testing::check_mesh(mesh_in);
        lagrange::orient_outward(mesh_in);
        REQUIRE(vertex_view(mesh_in) == vertex_view(mesh_out));
        REQUIRE(facet_view(mesh_in) == facet_view(mesh_out));
        lagrange::testing::check_mesh(mesh_in);
    }
}

TEST_CASE("orient_outward: cube with attrs", "[mesh][orient]")
{
    using Scalar = double;
    using Index = uint32_t;

    auto mesh_in =
        lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/simple/cube_flipped.fbx");

    lagrange::io::SaveOptions save_options;
    save_options.attribute_conversion_policy =
        lagrange::io::SaveOptions::AttributeConversionPolicy::ConvertAsNeeded;

    lagrange::seq_foreach_named_attribute_write(mesh_in, [&](auto name, auto&& attr) {
        if (name == "tangent") {
            attr.unsafe_set_usage(lagrange::AttributeUsage::Tangent);
        }
        if (name == "bitangent") {
            attr.unsafe_set_usage(lagrange::AttributeUsage::Bitangent);
        }
    });

    // Input attr for f0:
    // normal = (0, -1, 0)
    // tangent = (-1, 0, 0)
    // bitangent = (0, -1, 0)
    //
    // Expected after reorientation:
    // normal = (0, 1, 0)
    // tangent = (-1, 0, 0)
    // bitangent = (0, 1, 0)
    //
    // Every other facets should have its attributes unchanged.

    auto mesh_out = mesh_in;
    auto check_result = [&]() {
        lagrange::map_attribute_in_place(mesh_in, "normal", lagrange::AttributeElement::Corner);
        lagrange::map_attribute_in_place(mesh_in, "tangent", lagrange::AttributeElement::Corner);
        lagrange::map_attribute_in_place(mesh_in, "bitangent", lagrange::AttributeElement::Corner);
        lagrange::map_attribute_in_place(mesh_out, "normal", lagrange::AttributeElement::Corner);
        lagrange::map_attribute_in_place(mesh_out, "tangent", lagrange::AttributeElement::Corner);
        lagrange::map_attribute_in_place(mesh_out, "bitangent", lagrange::AttributeElement::Corner);
        auto normals_in = lagrange::attribute_matrix_view<Scalar>(mesh_in, "normal");
        auto tangents_in = lagrange::attribute_matrix_view<Scalar>(mesh_in, "tangent");
        auto bitangents_in = lagrange::attribute_matrix_view<Scalar>(mesh_in, "bitangent");
        auto normals_out = lagrange::attribute_matrix_view<Scalar>(mesh_out, "normal");
        auto tangents_out = lagrange::attribute_matrix_view<Scalar>(mesh_out, "tangent");
        auto bitangents_out = lagrange::attribute_matrix_view<Scalar>(mesh_out, "bitangent");
        Index nvpf = mesh_in.get_vertex_per_facet();
        Index num_corners = mesh_in.get_num_corners();
        for (Index c = 0; c < nvpf; ++c) {
            // Facet 0 is flipped
            REQUIRE(normals_in.row(c) == -normals_out.row(c));
            REQUIRE(tangents_in.row(c) == tangents_out.row(c));
            REQUIRE(bitangents_in.row(c) == -bitangents_out.row(c));
        }
        REQUIRE(
            normals_in.bottomRows(num_corners - nvpf) ==
            normals_out.bottomRows(num_corners - nvpf));
        REQUIRE(
            tangents_in.bottomRows(num_corners - nvpf) ==
            tangents_out.bottomRows(num_corners - nvpf));
        REQUIRE(
            bitangents_in.bottomRows(num_corners - nvpf) ==
            bitangents_out.bottomRows(num_corners - nvpf));
    };

    SECTION("indexed attrs")
    {
        lagrange::orient_outward(mesh_out);
        check_result();
    }
    SECTION("corner attrs")
    {
        lagrange::AttributeMatcher matcher;
        matcher.element_types = lagrange::AttributeElement::Indexed;
        auto ids = lagrange::find_matching_attributes(mesh_out, matcher);
        for (auto id : ids) {
            lagrange::map_attribute_in_place(mesh_out, id, lagrange::AttributeElement::Corner);
        }
        lagrange::orient_outward(mesh_out);
        check_result();
    }
    SECTION("facet attrs")
    {
        lagrange::AttributeMatcher matcher;
        matcher.element_types = lagrange::AttributeElement::Indexed;
        matcher.usages = ~lagrange::BitField(lagrange::AttributeUsage::UV);
        auto ids = lagrange::find_matching_attributes(mesh_out, matcher);
        for (auto id : ids) {
            lagrange::map_attribute_in_place(mesh_out, id, lagrange::AttributeElement::Facet);
        }
        lagrange::orient_outward(mesh_out);
        check_result();
    }
}

TEST_CASE("orient_outward: poly", "[mesh][orient]")
{
    using Scalar = double;
    using Index = uint32_t;

    std::vector<std::string> names = {"hexaSphere.obj", "noisy-sphere.obj"};
    std::vector<bool> is_positive = {true, false};

    for (size_t i = 0; i < names.size(); ++i) {
        for (auto with_edges : {false, true}) {
            auto mesh =
                lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/poly/" + names[i]);
            auto expected = mesh;
            if (with_edges) {
                mesh.initialize_edges();
            }

            // Should be a no-op
            lagrange::OrientOptions opt;
            opt.positive = is_positive[i];
            lagrange::orient_outward(mesh, opt);
            REQUIRE(vertex_view(mesh) == vertex_view(expected));
            REQUIRE(
                vector_view(mesh.get_corner_to_vertex()) ==
                vector_view(expected.get_corner_to_vertex()));
            lagrange::testing::check_mesh(mesh);

            // Randomly flip some facets
            mesh.flip_facets([](Index f) { return f % 10 == 0; });
            REQUIRE(vertex_view(mesh) == vertex_view(expected));
            REQUIRE(
                vector_view(mesh.get_corner_to_vertex()) !=
                vector_view(expected.get_corner_to_vertex()));

            // Orient should bring it back to its original state
            lagrange::orient_outward(mesh, opt);
            REQUIRE(vertex_view(mesh) == vertex_view(expected));
            REQUIRE(
                vector_view(mesh.get_corner_to_vertex()) ==
                vector_view(expected.get_corner_to_vertex()));
            lagrange::testing::check_mesh(mesh);
        }
    }
}

TEST_CASE("flip_facets: two triangles", "[mesh][flip]")
{
    using Index = uint32_t;

    for (auto with_edges : {false, true}) {
        lagrange::SurfaceMesh32f mesh;
        mesh.add_vertices(4);
        mesh.add_triangle(0, 1, 3);
        mesh.add_triangle(2, 3, 1);
        if (with_edges) {
            mesh.initialize_edges();
        }

        lagrange::testing::check_mesh(mesh);
        mesh.flip_facets([](Index f) { return f == 1; });
        lagrange::testing::check_mesh(mesh);
    }
}

TEST_CASE("flip_facets: poly", "[mesh][flip]")
{
    using Scalar = double;
    using Index = uint32_t;

    std::vector<std::string> names = {"poly/tetris.obj", "square.obj"};
    for (auto name : names) {
        for (auto with_edges : {false, true}) {
            auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/" + name);
            if (with_edges) {
                mesh.initialize_edges();
            }

            lagrange::testing::check_mesh(mesh);
            mesh.flip_facets([](Index f) { return f % 5 == 0; });
            lagrange::testing::check_mesh(mesh);
        }
    }
}

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS

TEST_CASE("orient_outward: legacy", "[mesh][orient]")
{
    auto mesh_in =
        lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/core/torus3_in.obj");
    auto mesh_out =
        lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/core/torus3_out.obj");

    lagrange::orient_outward(*mesh_in);
    REQUIRE(mesh_in->get_vertices() == mesh_out->get_vertices());
    REQUIRE(mesh_in->get_facets() == mesh_out->get_facets());
}

#endif

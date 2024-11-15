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
#include <lagrange/orient_outward.h>
#include <lagrange/testing/check_mesh.h>
#include <lagrange/utils/fmt_eigen.h>
#include <lagrange/views.h>

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

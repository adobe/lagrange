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

#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/compute_facet_area.h>
#include <lagrange/create_mesh.h>
#include <lagrange/mesh_cleanup/remove_duplicate_facets.h>
#include <lagrange/utils/safe_cast.h>

#include <catch2/catch_approx.hpp>

TEST_CASE("RemoveDuplicateFacetsTest", "[duplicate][cleanup][mesh]")
{
    using namespace lagrange;

    SECTION("Simple")
    {
        Vertices3D vertices(4, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0;
        Triangles facets(2, 3);
        facets << 0, 1, 2, 2, 1, 3;

        auto mesh = create_mesh(vertices, facets);
        mesh->initialize_uv(vertices.leftCols(2).eval(), facets);
        REQUIRE(mesh->is_uv_initialized());
        auto out_mesh = remove_duplicate_facets(*mesh);

        REQUIRE(out_mesh->get_num_vertices() == 4);
        REQUIRE(out_mesh->get_num_facets() == 2);
        REQUIRE(out_mesh->is_uv_initialized());
        REQUIRE(out_mesh->get_uv_indices().rows() == 2);

        auto uv_areas = compute_uv_area_raw(out_mesh->get_uv(), out_mesh->get_uv_indices());
        REQUIRE(uv_areas.sum() == Catch::Approx(1.0));
    }

    SECTION("Simple duplicates")
    {
        Vertices3D vertices(4, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0;
        Triangles facets(3, 3);
        facets << 0, 1, 2, 2, 1, 3, 1, 2, 3;

        auto mesh = create_mesh(vertices, facets);
        mesh->initialize_uv(vertices.leftCols(2).eval(), facets);
        REQUIRE(mesh->is_uv_initialized());

        using MeshType = decltype(mesh)::element_type;
        typename MeshType::AttributeArray corner_indices(9, 1);
        corner_indices << 0, 1, 2, 3, 4, 5, 6, 7, 8;
        mesh->add_corner_attribute("index");
        mesh->set_corner_attribute("index", corner_indices);

        auto out_mesh = remove_duplicate_facets(*mesh);

        REQUIRE(out_mesh->get_num_vertices() == 4);
        REQUIRE(out_mesh->get_num_facets() == 2);

        REQUIRE(out_mesh->has_corner_attribute("index"));
        const auto& out_corner_indices = out_mesh->get_corner_attribute("index");
        REQUIRE(out_corner_indices.rows() == 6);

        using Index = typename decltype(mesh)::element_type::Index;
        const auto& out_facets = out_mesh->get_facets();
        for (Index i = 0; i < 6; i++) {
            Index ori_i = safe_cast<Index>(std::round(out_corner_indices(i, 0)));
            REQUIRE(facets(ori_i / 3, ori_i % 3) == out_facets(i / 3, i % 3));
        }

        REQUIRE(out_mesh->is_uv_initialized());
        REQUIRE(out_mesh->get_uv_indices().rows() == 2);

        auto uv_areas = compute_uv_area_raw(out_mesh->get_uv(), out_mesh->get_uv_indices());
        REQUIRE(uv_areas.sum() == Catch::Approx(1.0));
    }

    SECTION("Empty mesh")
    {
        // Should not crash.
        Vertices3D vertices(4, 3);
        Triangles facets(0, 3);
        auto mesh = create_mesh(vertices, facets);
        mesh->initialize_uv(vertices.leftCols(2).eval(), facets);
        auto out_mesh = remove_duplicate_facets(*mesh);
        REQUIRE(out_mesh->get_num_vertices() == 4);
        REQUIRE(out_mesh->get_num_facets() == 0);

        REQUIRE(out_mesh->is_uv_initialized());
        REQUIRE(out_mesh->get_uv_indices().rows() == 0);
    }

    SECTION("plane")
    {
        // plane.obj has no duplicate facets.
        auto mesh = lagrange::testing::load_mesh<TriangleMesh3D>("open/core/plane.obj");
        auto out_mesh = remove_duplicate_facets(*mesh);
        REQUIRE(out_mesh->get_num_vertices() == mesh->get_num_vertices());
        REQUIRE(out_mesh->get_num_facets() == mesh->get_num_facets());
    }
}

TEST_CASE("RemoveDuplicateFacetsTest_slow", "[duplicate][cleanup][mesh]" LA_SLOW_FLAG)
{
    using namespace lagrange;

    SECTION("splash")
    {
        using Index = TriangleMesh3D::Index;
        auto mesh = lagrange::testing::load_mesh<TriangleMesh3D>("corp/core/splash_08_debug.obj");
        const auto vertex_per_facet = mesh->get_vertex_per_facet();
        REQUIRE(vertex_per_facet == 3);

        using MeshType = decltype(mesh)::element_type;
        using AttributeArray = typename MeshType::AttributeArray;

        const auto num_vertices = mesh->get_num_vertices();
        AttributeArray vertex_indices(num_vertices, 1);
        vertex_indices.col(0).setLinSpaced(0, num_vertices - 1);
        mesh->add_vertex_attribute("index");
        mesh->set_vertex_attribute("index", vertex_indices);

        const auto num_facets = mesh->get_num_facets();
        AttributeArray facet_indices(num_facets, 1);
        facet_indices.col(0).setLinSpaced(0, num_facets - 1);
        mesh->add_facet_attribute("index");
        mesh->set_facet_attribute("index", facet_indices);

        const auto num_corners = num_facets * vertex_per_facet;
        AttributeArray corner_indices(num_corners, 1);
        corner_indices.col(0).setLinSpaced(0, num_corners - 1);
        mesh->add_corner_attribute("index");
        mesh->set_corner_attribute("index", corner_indices);

        auto out_mesh = remove_duplicate_facets(*mesh);
        REQUIRE(out_mesh->get_num_vertices() == mesh->get_num_vertices());
        REQUIRE(out_mesh->get_num_facets() < mesh->get_num_facets());
        auto out_mesh_2 = remove_duplicate_facets(*out_mesh);
        REQUIRE(out_mesh_2->get_num_vertices() == out_mesh->get_num_vertices());
        REQUIRE(out_mesh_2->get_num_facets() == out_mesh->get_num_facets());

        REQUIRE(out_mesh_2->has_vertex_attribute("index"));
        REQUIRE(out_mesh_2->has_facet_attribute("index"));
        REQUIRE(out_mesh_2->has_corner_attribute("index"));

        const auto& in_vertices = mesh->get_vertices();
        const auto& out_vertices = out_mesh_2->get_vertices();
        const auto& in_facets = mesh->get_facets();
        const auto& out_facets = out_mesh_2->get_facets();

        const auto& out_vertex_indices = out_mesh_2->get_vertex_attribute("index");
        const auto& out_facet_indices = out_mesh_2->get_facet_attribute("index");
        const auto& out_corner_indices = out_mesh_2->get_corner_attribute("index");

        const auto num_out_vertices = out_vertices.rows();
        const auto num_out_facets = out_facets.rows();

        REQUIRE(out_vertex_indices.rows() == num_out_vertices);
        REQUIRE(out_facet_indices.rows() == num_out_facets);
        REQUIRE(out_corner_indices.rows() == num_out_facets * vertex_per_facet);

        for (Index i = 0; i < num_out_vertices; i++) {
            REQUIRE(out_vertex_indices(i, 0) == vertex_indices(i, 0));
        }
        for (Index i = 0; i < num_out_facets; i++) {
            const Index ori_fi = safe_cast<Index>(std::round(out_facet_indices(i, 0)));
            REQUIRE(ori_fi < num_facets);
            REQUIRE(in_facets.row(ori_fi) == out_facets.row(i));
            for (Index j = 0; j < vertex_per_facet; j++) {
                const Index out_ci = i * vertex_per_facet + j;
                const Index in_ci = safe_cast<Index>(std::round(out_corner_indices(out_ci, 0)));
                REQUIRE(
                    in_facets(in_ci / vertex_per_facet, in_ci % vertex_per_facet) ==
                    out_facets(i, j));

                const auto& in_v =
                    in_vertices.row(in_facets(in_ci / vertex_per_facet, in_ci % vertex_per_facet));
                const auto& out_v = out_vertices.row(
                    out_facets(out_ci / vertex_per_facet, out_ci % vertex_per_facet));
                REQUIRE(in_v == out_v);
            }
        }
    }
}

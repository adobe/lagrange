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
#include <catch2/catch_approx.hpp>

#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/compute_facet_area.h>
#include <lagrange/create_mesh.h>
#include <lagrange/mesh_cleanup/split_long_edges.h>

TEST_CASE("SplitLongEdgesTest", "[split_long_edges][triangle_mesh][cleanup]" LA_SLOW_DEBUG_FLAG)
{
    using namespace lagrange;
    Vertices3D vertices(3, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;
    Triangles facets(1, 3);
    facets << 0, 1, 2;

    using Scalar = typename Vertices3D::Scalar;

    auto mesh = create_mesh(vertices, facets);
    REQUIRE(mesh->get_num_vertices() == 3);
    REQUIRE(mesh->get_num_facets() == 1);

    mesh->initialize_uv(vertices.leftCols(2).eval(), facets);
    REQUIRE(mesh->is_uv_initialized());

    SECTION("no split")
    {
        auto mesh2 = split_long_edges(*mesh, 2.0);
        REQUIRE(mesh2->get_num_vertices() == 3);
        REQUIRE(mesh2->get_num_facets() == 1);

        const auto uv_areas = compute_uv_area_raw(mesh2->get_uv(), mesh2->get_uv_indices());
        REQUIRE(uv_areas.sum() == Catch::Approx(0.5));
    }

    SECTION("single split")
    {
        auto mesh2 = split_long_edges(*mesh, 1.5);
        REQUIRE(mesh2->get_num_vertices() == 4);
        REQUIRE(mesh2->get_num_facets() == 2);

        const auto uv_areas = compute_uv_area_raw(mesh2->get_uv(), mesh2->get_uv_indices());
        REQUIRE(uv_areas.sum() == Catch::Approx(0.5));
    }

    SECTION("Two triangles")
    {
        vertices.conservativeResize(4, 3);
        vertices.row(3) << 1.0, 1.0, 0.0;
        facets.resize(2, 3);
        facets << 0, 1, 2, 2, 1, 3;
        mesh->initialize_uv(vertices.leftCols(2).eval(), facets);
        REQUIRE(mesh->is_uv_initialized());

        mesh->import_vertices(vertices);
        mesh->import_facets(facets);

        SECTION("simple")
        {
            auto mesh2 = split_long_edges(*mesh, 1.5);
            REQUIRE(mesh2->get_num_vertices() == 5);
            REQUIRE(mesh2->get_num_facets() == 4);

            const auto uv_areas = compute_uv_area_raw(mesh2->get_uv(), mesh2->get_uv_indices());
            REQUIRE(uv_areas.sum() == Catch::Approx(1.0));
        }

        SECTION("with attribute")
        {
            using MeshType = decltype(mesh)::element_type;
            using IndexArray = typename MeshType::AttributeArray;
            IndexArray vertex_indices(4, 1);
            vertex_indices << 0, 1, 2, 3;
            IndexArray facet_indices(2, 1);
            facet_indices << 0, 1;

            mesh->add_vertex_attribute("index");
            mesh->set_vertex_attribute("index", vertex_indices);
            mesh->add_facet_attribute("index");
            mesh->set_facet_attribute("index", facet_indices);

            auto mesh2 = split_long_edges(*mesh, 1.5);
            REQUIRE(mesh2->get_num_vertices() == 5);
            REQUIRE(mesh2->get_num_facets() == 4);

            REQUIRE(mesh2->has_vertex_attribute("index"));
            REQUIRE(mesh2->has_facet_attribute("index"));

            const auto& v_idx = mesh2->get_vertex_attribute("index");
            const auto& f_idx = mesh2->get_facet_attribute("index");

            REQUIRE(v_idx.rows() == 5);
            REQUIRE(f_idx.rows() == 4);

            const auto& ori_vts = mesh->get_vertices();
            const auto& vts = mesh2->get_vertices();
            for (size_t i = 0; i < 5; i++) {
                Scalar int_part, frac_part;
                frac_part = modf(v_idx(i, 0), &int_part);
                if (frac_part == 0.0) {
                    REQUIRE(vts.row(i) == ori_vts.row(safe_cast<int>(int_part)));
                } else {
                    REQUIRE(v_idx(i, 0) == 1.5);
                }
            }

            REQUIRE(f_idx.minCoeff() == 0);
            REQUIRE(f_idx.maxCoeff() == 1);

            const auto uv_areas = compute_uv_area_raw(mesh2->get_uv(), mesh2->get_uv_indices());
            REQUIRE(uv_areas.sum() == Catch::Approx(1.0));
        }
    }

    SECTION("with normals")
    {
        using MeshType = decltype(mesh)::element_type;
        mesh = testing::load_mesh<MeshType>("open/core/bunny_simple.obj");
        REQUIRE(mesh->get_num_vertices() == 2503);
        REQUIRE(mesh->get_num_facets() == 5002);
        REQUIRE(mesh->has_corner_attribute("normal"));
        map_corner_attribute_to_indexed_attribute(*mesh, "normal");
        REQUIRE(mesh->has_indexed_attribute("normal"));

        auto mesh2 = split_long_edges(*mesh, 0.0001, true);
        REQUIRE(mesh2->has_indexed_attribute("normal"));
    }

    SECTION("with uv")
    {
        using MeshType = decltype(mesh)::element_type;
        mesh = testing::load_mesh<MeshType>("open/core/blub_open.obj");
        REQUIRE(mesh->get_num_vertices() == 5857);
        REQUIRE(mesh->get_num_facets() == 11648);
        REQUIRE(mesh->is_uv_initialized());
        auto mesh2 = split_long_edges(*mesh, 0.0001, true);
        REQUIRE(mesh2->is_uv_initialized());
    }
}

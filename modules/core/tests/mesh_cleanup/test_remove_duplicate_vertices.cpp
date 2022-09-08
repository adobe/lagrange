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

#include <lagrange/Mesh.h>
#include <lagrange/attributes/attribute_utils.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/mesh_cleanup/remove_duplicate_vertices.h>


TEST_CASE("RemoveDuplicateVerticesTest", "[duplicate][duplicate_vertices][cleanup]")
{
    using namespace lagrange;
    Vertices3D vertices(4, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0;
    Triangles facets(2, 3);
    facets << 0, 1, 2, 2, 1, 3;

    auto mesh = create_mesh(vertices, facets);
    REQUIRE(mesh->get_num_vertices() == 4);
    REQUIRE(mesh->get_num_facets() == 2);

    using MeshType = decltype(mesh)::element_type;
    using AttributeArray = typename MeshType::AttributeArray;

    SECTION("simple")
    {
        mesh->initialize_uv(vertices.leftCols(2).eval(), facets);
        REQUIRE(mesh->is_uv_initialized());

        auto mesh2 = remove_duplicate_vertices(*mesh);

        SECTION("repeated call")
        {
            for (size_t i = 0; i < 5; i++) {
                mesh2 = remove_duplicate_vertices(*mesh2);
            }
        }

        REQUIRE(mesh2->get_num_vertices() == 3);
        // Both facets are overlapping but with oppositive orientations.
        REQUIRE(mesh2->get_num_facets() == 2);
        REQUIRE(mesh2->is_uv_initialized());
        REQUIRE(mesh2->get_uv_indices().rows() == 2);
    }

    SECTION("Dupicated facets")
    {
        facets << 0, 1, 2, 3, 1, 2;

        mesh->initialize_uv(vertices.leftCols(2).eval(), facets);
        REQUIRE(mesh->is_uv_initialized());

        mesh->import_facets(facets);

        auto mesh2 = remove_duplicate_vertices(*mesh);
        REQUIRE(mesh2->get_num_vertices() == 3);
        // Both facets are overlapping and with the same orientation.
        // Make sure non-topologically degenerate facets are left alone.
        REQUIRE(mesh2->get_num_facets() == 2);
        REQUIRE(mesh2->is_uv_initialized());
        REQUIRE(mesh2->get_uv_indices().rows() == 2);
    }

    SECTION("Single point")
    {
        vertices << 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0;
        typename MeshType::UVArray uv = vertices.leftCols(2);
        mesh->import_vertices(vertices);
        mesh->initialize_uv(uv, facets);
        REQUIRE(mesh->is_uv_initialized());

        auto mesh2 = remove_duplicate_vertices(*mesh);
        REQUIRE(mesh2->get_num_vertices() == 1);
        // All facets are topologically degenerate, thus removed.
        REQUIRE(mesh2->get_num_facets() == 0);

        REQUIRE(mesh2->is_uv_initialized());
        REQUIRE(mesh2->get_uv_indices().rows() == 0);
    }

    SECTION("Empty mesh")
    {
        // Should not crash.
        facets.resize(0, 3);
        mesh->import_facets(facets);
        auto mesh2 = remove_duplicate_vertices(*mesh);
        REQUIRE(mesh2->get_num_vertices() == 3);
        REQUIRE(mesh2->get_num_facets() == 0);
    }

    SECTION("Attributes")
    {
        AttributeArray vertex_indices(mesh->get_num_vertices(), 1);
        for (int i = 0; i < vertex_indices.size(); i++) {
            vertex_indices(i) = i;
        }
        mesh->add_vertex_attribute("index");
        mesh->set_vertex_attribute("index", vertex_indices);

        REQUIRE(mesh->has_vertex_attribute("index"));
        auto mesh2 = remove_duplicate_vertices(*mesh);

        REQUIRE(mesh2->has_vertex_attribute("index"));
        const auto& attr = mesh2->get_vertex_attribute("index");
        REQUIRE(attr.size() == 3);
        const auto& ori_vts = mesh->get_vertices();
        const auto& vts = mesh2->get_vertices();
        for (size_t i = 0; i < 3; i++) {
            REQUIRE(vts.row(i) == ori_vts.row(size_t(attr(i, 0))));
        }
    }

    SECTION("With keys")
    {
        AttributeArray keys(4, 2);
        keys << 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0;
        mesh->add_vertex_attribute("keys");
        mesh->set_vertex_attribute("keys", keys);

        auto mesh2 = remove_duplicate_vertices(*mesh, "keys");
        REQUIRE(mesh2->get_num_vertices() == 4);
        REQUIRE(mesh2->get_num_facets() == 2);
    }

    SECTION("bug")
    {
        auto mesh2 = lagrange::testing::load_mesh<TriangleMesh3D>("open/core/cube_soup.obj");
        REQUIRE(12 == mesh2->get_num_facets());
        REQUIRE(24 == mesh2->get_num_vertices());
        REQUIRE(mesh2->is_uv_initialized());
        const auto& uv = mesh2->get_uv();
        const auto& uv_indices = mesh2->get_uv_indices();
        TriangleMesh3D::AttributeArray attr(12 * 3, 2);
        for (auto i : range(12)) {
            attr.row(i * 3 + 0) = uv.row(uv_indices(i, 0));
            attr.row(i * 3 + 1) = uv.row(uv_indices(i, 1));
            attr.row(i * 3 + 2) = uv.row(uv_indices(i, 2));
        }
        mesh2->add_corner_attribute("uv");
        mesh2->set_corner_attribute("uv", attr);

        auto mesh3 = remove_duplicate_vertices(*mesh2, "", false);
        REQUIRE(12 == mesh3->get_num_facets());
        REQUIRE(8 == mesh3->get_num_vertices());
        REQUIRE(mesh3->has_corner_attribute("uv"));
        const auto& corner_uv = mesh3->get_corner_attribute("uv");
        map_corner_attribute_to_indexed_attribute(*mesh3, "uv");

        REQUIRE(mesh3->is_uv_initialized());
        const auto& new_uv = mesh3->get_uv();
        const auto& new_uv_indices = mesh3->get_uv_indices();

        for (auto i : range(12)) {
            const auto f_old = mesh2->get_facets().row(i);
            const auto f_new = mesh3->get_facets().row(i);
            for (auto j : range(3)) {
                const auto v_old = mesh2->get_vertices().row(f_old[j]);
                const auto v_new = mesh3->get_vertices().row(f_new[j]);
                REQUIRE(v_old == v_new);
            }
        }


        for (auto i : range(12)) {
            REQUIRE(uv.row(uv_indices(i, 0)) == corner_uv.row(i * 3 + 0));
            REQUIRE(uv.row(uv_indices(i, 1)) == corner_uv.row(i * 3 + 1));
            REQUIRE(uv.row(uv_indices(i, 2)) == corner_uv.row(i * 3 + 2));

            REQUIRE(uv.row(uv_indices(i, 0)) == new_uv.row(new_uv_indices(i, 0)));
            REQUIRE(uv.row(uv_indices(i, 1)) == new_uv.row(new_uv_indices(i, 1)));
            REQUIRE(uv.row(uv_indices(i, 2)) == new_uv.row(new_uv_indices(i, 2)));
        }
    }

    SECTION("Multiple keys")
    {
        vertices.resize(6, 3);
        vertices << 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0;
        facets.resize(2, 3);
        facets << 0, 1, 2, 3, 4, 5;
        mesh = create_mesh(vertices, facets);
        mesh->initialize_uv(vertices.leftCols(2), facets);

        SECTION("Should merge")
        {
            auto out_mesh = remove_duplicate_vertices(*mesh, "", true);
            REQUIRE(out_mesh->get_num_vertices() == 4);
            REQUIRE(out_mesh->get_num_facets() == 2);
            REQUIRE(out_mesh->is_uv_initialized());
        }
        SECTION("Should not merge")
        {
            Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> vertex_ids(6, 1);
            vertex_ids << 1, 2, 3, 4, 5, 6;
            mesh->add_vertex_attribute("id");
            mesh->import_vertex_attribute("id", vertex_ids);

            auto out_mesh = remove_duplicate_vertices(*mesh, "id", true);
            REQUIRE(out_mesh->get_num_vertices() == 6);
            REQUIRE(out_mesh->get_num_facets() == 2);
            REQUIRE(out_mesh->is_uv_initialized());
        }
        SECTION("Should merge only 1 vertex")
        {
            Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> vertex_ids(6, 1);
            vertex_ids << 1, 2, 3, 3, 5, 6;
            mesh->add_vertex_attribute("id");
            mesh->import_vertex_attribute("id", vertex_ids);

            auto out_mesh = remove_duplicate_vertices(*mesh, "id", true);
            REQUIRE(out_mesh->get_num_vertices() == 5);
            REQUIRE(out_mesh->get_num_facets() == 2);
            REQUIRE(out_mesh->is_uv_initialized());
        }
    }
}

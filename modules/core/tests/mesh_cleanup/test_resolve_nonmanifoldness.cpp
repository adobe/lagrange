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
#include <iostream>

#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/mesh_cleanup/resolve_nonmanifoldness.h>


TEST_CASE("resolve_manifoldness", "[nonmanifold][Mesh][cleanup]")
{
    using namespace lagrange;

    SECTION("single triangle")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 3> vertices(3, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;
        Eigen::Matrix<int, Eigen::Dynamic, 3> facets(1, 3);
        facets << 0, 1, 2;

        auto in_mesh = lagrange::create_mesh(vertices, facets);
        in_mesh->initialize_topology();
        REQUIRE(in_mesh->is_vertex_manifold());

        in_mesh->initialize_uv(vertices.leftCols(2).eval(), facets);
        REQUIRE(in_mesh->is_uv_initialized());

        in_mesh->initialize_connectivity();

        auto out_mesh = lagrange::resolve_nonmanifoldness(*in_mesh);
        out_mesh->initialize_topology();
        REQUIRE(out_mesh->is_vertex_manifold());

        REQUIRE(out_mesh->is_uv_initialized());
        REQUIRE(out_mesh->get_uv_indices().rows() == 1);
    }

    SECTION("two triangles")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 3> vertices(4, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0;
        Eigen::Matrix<int, Eigen::Dynamic, 3> facets(2, 3);
        facets << 0, 1, 2, 1, 0, 3;

        auto in_mesh = lagrange::create_mesh(vertices, facets);
        in_mesh->initialize_topology();
        REQUIRE(in_mesh->is_vertex_manifold());

        in_mesh->initialize_uv(vertices.leftCols(2).eval(), facets);
        REQUIRE(in_mesh->is_uv_initialized());

        in_mesh->initialize_connectivity();

        auto out_mesh = lagrange::resolve_nonmanifoldness(*in_mesh);
        out_mesh->initialize_topology();
        REQUIRE(out_mesh->is_vertex_manifold());

        REQUIRE(out_mesh->is_uv_initialized());
        REQUIRE(out_mesh->get_uv_indices().rows() == 2);
    }

    SECTION("two inconsistely oriented triangles")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 3> vertices(4, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0;
        Eigen::Matrix<int, Eigen::Dynamic, 3> facets(2, 3);
        facets << 0, 1, 2, 0, 1, 3;

        auto in_mesh = lagrange::create_mesh(vertices, facets);
        in_mesh->initialize_components();
        in_mesh->initialize_topology();
        REQUIRE(!in_mesh->is_vertex_manifold());
        REQUIRE(in_mesh->is_edge_manifold());
        REQUIRE(in_mesh->get_num_components() == 1);

        in_mesh->initialize_connectivity();

        in_mesh->initialize_uv(vertices.leftCols(2).eval(), facets);
        REQUIRE(in_mesh->is_uv_initialized());

        auto out_mesh = lagrange::resolve_nonmanifoldness(*in_mesh);
        out_mesh->initialize_components();
        out_mesh->initialize_topology();
        REQUIRE(out_mesh->is_vertex_manifold());
        REQUIRE(out_mesh->get_num_components() == 2);

        REQUIRE(out_mesh->is_uv_initialized());
        REQUIRE(out_mesh->get_uv_indices().rows() == 2);
    }

    SECTION("three triangles nonmanifold")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 3> vertices(4, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0;
        Eigen::Matrix<int, Eigen::Dynamic, 3> facets(3, 3);
        facets << 0, 1, 2, 0, 1, 3, 1, 0, 2;

        auto in_mesh = lagrange::create_mesh(vertices, facets);
        in_mesh->initialize_topology();
        REQUIRE(!in_mesh->is_vertex_manifold());

        in_mesh->initialize_connectivity();

        in_mesh->initialize_uv(vertices.leftCols(2).eval(), facets);
        REQUIRE(in_mesh->is_uv_initialized());

        auto out_mesh = lagrange::resolve_nonmanifoldness(*in_mesh);
        out_mesh->initialize_topology();
        REQUIRE(out_mesh->is_vertex_manifold());

        REQUIRE(out_mesh->is_uv_initialized());
        REQUIRE(out_mesh->get_uv_indices().rows() == out_mesh->get_num_facets());
    }

    SECTION("nonmanifold tet")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 3> vertices(5, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, -1.0, 0.0, 0.0;
        Eigen::Matrix<int, Eigen::Dynamic, 3> facets(5, 3);
        facets << 0, 2, 1, 0, 3, 2, 0, 1, 3, 1, 2, 3, 0, 4, 3;

        auto in_mesh = lagrange::create_mesh(vertices, facets);
        in_mesh->initialize_topology();
        REQUIRE(!in_mesh->is_vertex_manifold());

        in_mesh->initialize_connectivity();

        Eigen::Matrix<double, Eigen::Dynamic, 2> uv(3, 2);
        uv << 0.0, 0.0, 1.0, 0.0, 0.0, 1.0;
        Eigen::Matrix<int, Eigen::Dynamic, 3> uv_indices(5, 3);
        uv_indices << 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2;
        in_mesh->initialize_uv(uv, uv_indices);
        REQUIRE(in_mesh->is_uv_initialized());

        auto out_mesh = lagrange::resolve_nonmanifoldness(*in_mesh);
        out_mesh->initialize_topology();
        REQUIRE(out_mesh->get_num_vertices() == 7);
        REQUIRE(out_mesh->is_vertex_manifold());

        REQUIRE(out_mesh->is_uv_initialized());
        REQUIRE(out_mesh->get_uv_indices().rows() == out_mesh->get_num_facets());
    }

    SECTION("vertex nonmanifold")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 3> vertices(7, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, -1.0, 0.0, 0.0, 0.0,
            -1.0, 0.0, 0.0, 0.0, -1.0;
        Eigen::Matrix<int, Eigen::Dynamic, 3> facets(8, 3);
        facets << 0, 2, 1, 0, 3, 2, 0, 1, 3, 1, 2, 3, 0, 6, 4, 0, 4, 5, 0, 5, 6, 6, 5, 4;

        auto in_mesh = lagrange::create_mesh(vertices, facets);
        in_mesh->initialize_topology();

        using IndexArray = typename decltype(in_mesh)::element_type::AttributeArray;
        IndexArray vertex_indices(7, 1);
        vertex_indices << 0, 1, 2, 3, 4, 5, 6;
        in_mesh->add_vertex_attribute("indices");
        in_mesh->set_vertex_attribute("indices", vertex_indices);
        IndexArray facet_indices(8, 1);
        facet_indices << 0, 1, 2, 3, 4, 5, 6, 7;
        in_mesh->add_facet_attribute("indices");
        in_mesh->set_facet_attribute("indices", facet_indices);

        REQUIRE(!in_mesh->is_vertex_manifold());

        in_mesh->initialize_connectivity();

        auto out_mesh = lagrange::resolve_nonmanifoldness(*in_mesh);
        out_mesh->initialize_topology();
        REQUIRE(out_mesh->get_num_vertices() == 8);
        REQUIRE(out_mesh->is_vertex_manifold());

        REQUIRE(out_mesh->has_vertex_attribute("indices"));
        REQUIRE(out_mesh->has_facet_attribute("indices"));

        const auto& out_vertex_indices = out_mesh->get_vertex_attribute("indices");
        REQUIRE(out_vertex_indices.rows() == 8);
        REQUIRE(out_vertex_indices.cols() == 1);

        const auto& out_vertices = out_mesh->get_vertices();
        for (int i = 0; i < 8; i++) {
            REQUIRE(
                (out_vertices.row(i) - vertices.row(safe_cast<int>(out_vertex_indices(i, 0))))
                    .norm() == 0.0);
        }

        const auto& out_facet_indices = out_mesh->get_facet_attribute("indices");
        REQUIRE(out_facet_indices.rows() == 8);
        REQUIRE(out_facet_indices.cols() == 1);
    }

    SECTION("topologically degenerated faces")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 3> vertices(4, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, -1.0, 1.0;
        Eigen::Matrix<int, Eigen::Dynamic, 3> facets(5, 3);
        facets << 0, 1, 2, 0, 3, 1, 1, 1, 2, 1, 1, 3, 1, 1, 1;
        auto mesh = lagrange::create_mesh(vertices, facets);
        mesh->initialize_topology();
        REQUIRE(!mesh->is_vertex_manifold());

        mesh->initialize_connectivity();

        mesh = lagrange::resolve_nonmanifoldness(*mesh);
        mesh->initialize_topology();
        REQUIRE(mesh->is_edge_manifold());
        REQUIRE(mesh->is_vertex_manifold());
    }

    SECTION("topologically degenerated faces 2")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 3> vertices(5, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, -1.0, 0.0, -1.0, 0.0, 0.0;
        Eigen::Matrix<int, Eigen::Dynamic, 3> facets(5, 3);
        facets << 0, 1, 2, 0, 3, 1, 1, 1, 2, 1, 1, 3, 1, 1, 1;
        auto mesh = lagrange::create_mesh(vertices, facets);
        mesh->initialize_topology();
        REQUIRE(!mesh->is_vertex_manifold());

        mesh->initialize_connectivity();

        mesh = lagrange::resolve_nonmanifoldness(*mesh);
        mesh->initialize_topology();
        REQUIRE(mesh->is_edge_manifold());
        REQUIRE(mesh->is_vertex_manifold());
    }
}

TEST_CASE("resolve_manifoldness_slow", "[nonmanifold][Mesh]" LA_SLOW_FLAG LA_CORP_FLAG)
{
    using namespace lagrange;

    SECTION("splash")
    {
        auto mesh = lagrange::testing::load_mesh<TriangleMesh3D>("corp/core/splash_08_debug.obj");

        mesh->initialize_connectivity();

        mesh = lagrange::resolve_nonmanifoldness(*mesh);
        mesh->initialize_topology();
        REQUIRE(mesh->is_vertex_manifold());
    }

    SECTION("desk")
    {
        auto mesh = lagrange::testing::load_mesh<TriangleMesh3D>("corp/core/z_desk_full_mockup.obj");
        mesh->initialize_topology();
        REQUIRE(!mesh->is_vertex_manifold());

        mesh->initialize_connectivity();

        mesh = lagrange::resolve_nonmanifoldness(*mesh);
        mesh->initialize_topology();
        REQUIRE(mesh->is_vertex_manifold());
    }
}

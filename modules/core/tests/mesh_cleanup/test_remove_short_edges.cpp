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

#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/mesh_cleanup/remove_short_edges.h>
#include <lagrange/mesh_cleanup/split_long_edges.h>


TEST_CASE("RemoveShortEdges", "[short_edges][cleanup]")
{
    using namespace lagrange;

    SECTION("Single triangle")
    {
        Vertices3D vertices(3, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;

        Triangles facets(1, 3);
        facets << 0, 1, 2;

        auto mesh = create_mesh(vertices, facets);
        auto mesh2 = remove_short_edges(*mesh, 0.5);
        auto mesh3 = remove_short_edges(*mesh, 2.0);

        REQUIRE(mesh->get_num_vertices() == mesh2->get_num_vertices());
        REQUIRE(mesh->get_num_facets() == mesh2->get_num_facets());
        REQUIRE((mesh->get_vertices() - mesh2->get_vertices()).norm() == 0.0);

        REQUIRE(mesh3->get_num_vertices() == 0);
        REQUIRE(mesh3->get_num_facets() == 0);
    }

    SECTION("two triangle")
    {
        Vertices3D vertices(4, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, -0.1;

        Triangles facets(4, 3);
        facets << 0, 1, 2, 1, 0, 3, 2, 1, 3, 0, 2, 3;

        auto mesh = create_mesh(vertices, facets);
        auto mesh2 = remove_short_edges(*mesh, 0.5);

        REQUIRE(3 == mesh2->get_num_vertices());
        REQUIRE(2 == mesh2->get_num_facets()); // Two oppositely oriented facets.
    }

    SECTION("zero edges")
    {
        Vertices3D vertices(4, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0;

        Triangles facets(2, 3);
        facets << 0, 1, 2, 0, 3, 1;

        auto mesh = create_mesh(vertices, facets);
        auto mesh2 = remove_short_edges(*mesh);

        REQUIRE(3 == mesh2->get_num_vertices());
        REQUIRE(1 == mesh2->get_num_facets());
    }

    SECTION("topologically degeneracy")
    {
        Vertices3D vertices(4, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0;

        Triangles facets(3, 3);
        facets << 0, 1, 2, 0, 3, 1, 3, 3, 2;

        auto mesh = create_mesh(vertices, facets);
        auto mesh2 = remove_short_edges(*mesh);

        REQUIRE(3 == mesh2->get_num_vertices());
        REQUIRE(1 == mesh2->get_num_facets());
    }

    SECTION("Narrow triangles")
    {
        constexpr double tol = 0.01;
        auto mesh = lagrange::testing::load_mesh<TriangleMesh3D>("open/core/narrow_triangles.obj");
        auto mesh2 = remove_short_edges(*mesh, tol);
        mesh2->initialize_topology();
        REQUIRE(mesh2->is_vertex_manifold());

        compute_edge_lengths(*mesh2);
        const auto& edge_lengths = mesh2->get_edge_attribute("length");
        REQUIRE(edge_lengths.rows() == mesh2->get_num_edges());
        REQUIRE(edge_lengths.minCoeff() > tol);
    }

    SECTION("Densely sampled triangle")
    {
        constexpr double tol = 0.1;
        Vertices3D vertices(3, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;

        Triangles facets(1, 3);
        facets << 0, 1, 2;

        auto mesh = create_mesh(vertices, facets);
        mesh = split_long_edges(*mesh, 4 * tol * tol, true);
        mesh = remove_short_edges(*mesh, tol);

        compute_edge_lengths(*mesh);
        const auto& edge_lengths = mesh->get_edge_attribute("length");
        REQUIRE(edge_lengths.rows() == mesh->get_num_edges());
        REQUIRE(edge_lengths.minCoeff() > tol);
    }

    SECTION("Stress test")
    {
        // Reducing 10k triangles to 1 triangle.
        constexpr int N = 10000;
        Vertices3D vertices(N + 2, 3);
        vertices.setZero();
        vertices.row(0) << 1.0, 0.0, 0.0;
        vertices.row(1) << 0.0, 1.0, 0.0;
        Triangles facets(N, 3);
        facets.row(0) << 0, 1, 2;
        for (auto i : range(1, N)) {
            facets.row(i) << 1, i + 1, i + 2;
        }
        REQUIRE(facets.maxCoeff() < N + 2);

        auto mesh = create_mesh(vertices, facets);
        mesh = remove_short_edges(*mesh);

        REQUIRE(mesh->get_num_facets() == 1);
        REQUIRE(mesh->get_num_vertices() == 3);
    }

    SECTION("Attributes")
    {
        Vertices3D vertices(4, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0;

        Triangles facets(3, 3);
        facets << 0, 1, 2, 0, 3, 1, 3, 3, 2;

        auto mesh = create_mesh(vertices, facets);

        using MeshType = decltype(mesh)::element_type;
        using AttributeArray = typename MeshType::AttributeArray;

        AttributeArray facet_indices(3, 1);
        facet_indices << 0, 1, 2;
        mesh->add_facet_attribute("index");
        mesh->set_facet_attribute("index", facet_indices);

        auto mesh2 = remove_short_edges(*mesh);

        REQUIRE(3 == mesh2->get_num_vertices());
        REQUIRE(1 == mesh2->get_num_facets());

        REQUIRE(mesh2->has_facet_attribute("index"));
        const auto& attr = mesh2->get_facet_attribute("index");
        REQUIRE(attr.rows() == 1);
        REQUIRE(attr(0, 0) == 0);
    }
}

TEST_CASE("RemoveShortEdges2", "[short_edges][cleanup]" LA_CORP_FLAG)
{
    using namespace lagrange;
    SECTION("EUC_10594")
    {
        auto mesh = lagrange::testing::load_mesh<TriangleMesh3D>("corp/core/EUC_10594.obj");
        auto mesh2 = remove_short_edges(*mesh);
        compute_edge_lengths(*mesh2);
        const auto& edge_lengths = mesh2->get_edge_attribute("length");
        REQUIRE(edge_lengths.minCoeff() > 0.0);
    }
}

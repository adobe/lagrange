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
#include <lagrange/mesh_cleanup/remove_isolated_vertices.h>
#include <lagrange/utils/range.h>

TEST_CASE("RemoveIsolatedVertices", "[isolated_vertices][cleanup]")
{
    using namespace lagrange;
    Vertices3D vertices(4, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0;
    Triangles facets(2, 3);

    SECTION("No isolated vertices")
    {
        facets << 0, 1, 2, 2, 1, 3;
        auto mesh = create_mesh(vertices, facets);
        auto mesh2 = remove_isolated_vertices(*mesh);
        REQUIRE(mesh->get_num_vertices() == mesh2->get_num_vertices());
        REQUIRE(mesh->get_num_facets() == mesh2->get_num_facets());

        const auto facets2 = mesh2->get_facets();
        REQUIRE(facets2.minCoeff() >= 0);
        REQUIRE(facets2.maxCoeff() < mesh2->get_num_vertices());
    }

    SECTION("Single isolated vertices")
    {
        facets << 0, 1, 2, 2, 1, 0;
        auto mesh = create_mesh(vertices, facets);
        auto mesh2 = remove_isolated_vertices(*mesh);
        REQUIRE(mesh->get_num_vertices() - 1 == mesh2->get_num_vertices());
        REQUIRE(mesh->get_num_facets() == mesh2->get_num_facets());

        const auto facets2 = mesh2->get_facets();
        REQUIRE(facets2.minCoeff() >= 0);
        REQUIRE(facets2.maxCoeff() < mesh2->get_num_vertices());
    }

    SECTION("Multiple isolated vertices")
    {
        vertices.resize(5, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0;
        facets << 0, 1, 2, 2, 1, 0;
        auto mesh = create_mesh(vertices, facets);
        auto mesh2 = remove_isolated_vertices(*mesh);
        REQUIRE(3 == mesh2->get_num_vertices());
        REQUIRE(2 == mesh2->get_num_facets());

        const auto facets2 = mesh2->get_facets();
        REQUIRE(facets2.minCoeff() >= 0);
        REQUIRE(facets2.maxCoeff() < mesh2->get_num_vertices());
    }

    SECTION("All vertices are isolated")
    {
        vertices.resize(5, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0;
        facets.resize(0, 3);
        auto mesh = create_mesh(vertices, facets);
        auto mesh2 = remove_isolated_vertices(*mesh);
        REQUIRE(0 == mesh2->get_num_vertices());
        REQUIRE(0 == mesh2->get_num_facets());
    }

    SECTION("Vertex attributes")
    {
        facets << 1, 3, 2, 2, 3, 1;
        auto mesh = create_mesh(vertices, facets);
        const int num_vertices = mesh->get_num_vertices();
        using MeshType = decltype(mesh)::element_type;
        using AttributeArray = typename MeshType::AttributeArray;
        AttributeArray index(num_vertices, 1);
        for (int i = 0; i < num_vertices; i++) {
            index(i, 0) = safe_cast<int>(i);
        }
        mesh->add_vertex_attribute("index");
        mesh->set_vertex_attribute("index", index);

        auto mesh2 = remove_isolated_vertices(*mesh);
        REQUIRE(mesh2->has_vertex_attribute("index"));

        const auto attr2 = mesh2->get_vertex_attribute("index");
        REQUIRE(3 == attr2.rows());
        REQUIRE(attr2.minCoeff() == 1);
        REQUIRE(attr2.maxCoeff() == 3);
    }

    SECTION("Facet attributes")
    {
        facets << 1, 3, 2, 2, 3, 1;
        auto mesh = create_mesh(vertices, facets);
        const int num_facets = mesh->get_num_facets();
        using MeshType = decltype(mesh)::element_type;
        using AttributeArray = typename MeshType::AttributeArray;
        AttributeArray index(num_facets, 1);
        for (int i = 0; i < num_facets; i++) {
            index(i, 0) = safe_cast<int>(i);
        }
        mesh->add_facet_attribute("index");
        mesh->set_facet_attribute("index", index);

        auto mesh2 = remove_isolated_vertices(*mesh);
        REQUIRE(mesh2->has_facet_attribute("index"));

        const auto attr2 = mesh2->get_facet_attribute("index");
        REQUIRE((index - attr2).norm() == Approx(0.0));
    }

    SECTION("Corner attributes")
    {
        facets << 1, 3, 2, 2, 3, 1;
        auto mesh = create_mesh(vertices, facets);
        const size_t num_facets = mesh->get_num_facets();
        const size_t vertex_per_facet = mesh->get_vertex_per_facet();
        using MeshType = decltype(mesh)::element_type;
        using Scalar = typename MeshType::Scalar;
        using AttributeArray = typename MeshType::AttributeArray;
        AttributeArray index(num_facets * vertex_per_facet, 1);
        for (auto i : range(num_facets)) {
            for (auto j : range(vertex_per_facet))
                index(i * vertex_per_facet + j, 0) = safe_cast<Scalar>(i);
        }
        mesh->add_corner_attribute("index");
        mesh->set_corner_attribute("index", index);

        auto mesh2 = remove_isolated_vertices(*mesh);
        REQUIRE(mesh2->has_corner_attribute("index"));

        const auto attr2 = mesh2->get_corner_attribute("index");
        REQUIRE((index - attr2).norm() == Approx(0.0));
    }
}

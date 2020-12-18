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
#include <lagrange/mesh_cleanup/remove_topologically_degenerate_triangles.h>

TEST_CASE("RemoveTopologicallyDegenerateTriangles", "[degenerate][triangle_mesh][cleanup]")
{
    using namespace lagrange;
    Vertices3D vertices(3, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;
    Triangles facets(1, 3);
    facets << 0, 1, 2;

    auto mesh = create_mesh(vertices, facets);
    REQUIRE(mesh->get_num_vertices() == 3);
    REQUIRE(mesh->get_num_facets() == 1);

    using MeshType = decltype(mesh)::element_type;
    using AttributeArray = typename MeshType::AttributeArray;

    SECTION("No degeneracy")
    {
        auto mesh2 = remove_topologically_degenerate_triangles(*mesh);
        REQUIRE(mesh2->get_num_vertices() == 3);
        REQUIRE(mesh2->get_num_facets() == 1);
    }

    SECTION("Simple")
    {
        facets.resize(2, 3);
        facets << 0, 1, 2, 0, 0, 0;
        mesh->import_facets(facets);
        auto mesh2 = remove_topologically_degenerate_triangles(*mesh);
        REQUIRE(mesh2->get_num_vertices() == 3);
        REQUIRE(mesh2->get_num_facets() == 1);
    }

    SECTION("All facets are degenerate")
    {
        facets.resize(2, 3);
        facets << 0, 1, 1, 0, 0, 2;
        mesh->import_facets(facets);
        auto mesh2 = remove_topologically_degenerate_triangles(*mesh);
        REQUIRE(mesh2->get_num_vertices() == 3);
        REQUIRE(mesh2->get_num_facets() == 0);
    }

    SECTION("Facet attributes")
    {
        facets.resize(3, 3);
        facets << 0, 1, 1, 0, 1, 2, 0, 0, 2;
        mesh->import_facets(facets);

        AttributeArray facet_indices(3, 1);
        facet_indices << 0, 1, 2;
        mesh->add_facet_attribute("index");
        mesh->set_facet_attribute("index", facet_indices);

        auto mesh2 = remove_topologically_degenerate_triangles(*mesh);
        REQUIRE(mesh2->get_num_vertices() == 3);
        REQUIRE(mesh2->get_num_facets() == 1);
        REQUIRE(mesh2->has_facet_attribute("index"));

        const auto& attr = mesh2->get_facet_attribute("index");
        REQUIRE(attr.rows() == 1);
        REQUIRE(attr(0, 0) == 1);
    }
}

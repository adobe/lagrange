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
// Since test_remove_duplicate_vertices already tests this functionality,
// it is only mildly tested here.

#include <lagrange/testing/common.h>

#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/reorder_mesh_vertices.h>

TEST_CASE("ReorderMeshVertices", "[mesh][reorder_vertices]")
{
    using namespace lagrange;
    Vertices3D vertices(4, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0;
    Triangles facets(2, 3);
    facets << 0, 1, 2, 2, 1, 3;
    using Index = Triangles::Scalar;

    auto mesh = create_mesh(vertices, facets);
    REQUIRE(mesh->get_num_vertices() == 4);
    REQUIRE(mesh->get_num_facets() == 2);

    SECTION("All collpse to one")
    {
        // All vertices should now be zero!
        auto mesh2 = reorder_mesh_vertices(*mesh, {INVALID<Index>(), 0, 0, 0});
        CHECK(mesh2->get_num_vertices() == 1);
        CHECK(mesh2->get_num_facets() == mesh->get_num_facets());
        CHECK(mesh2->get_facets().maxCoeff() == 0);
        CHECK(mesh2->get_facets().minCoeff() == 0);
    }

    SECTION("Nothing should happen")
    {
        // Invalid means keep the index
        std::vector<Index> forward_mapping = {INVALID<Index>(), INVALID<Index>(), 2, 3};
        auto mesh2 = reorder_mesh_vertices(*mesh, forward_mapping);
        REQUIRE(mesh2->get_num_vertices() == mesh->get_num_vertices());
        REQUIRE(mesh2->get_num_facets() == mesh->get_num_facets());
        CHECK(mesh2->get_facets() == mesh->get_facets());
        CHECK(mesh2->get_vertices() == mesh->get_vertices());
    }

    SECTION("Only two points should remain")
    {
        std::vector<Index> forward_mapping = {1, 1, 0, 0};
        auto mesh2 = reorder_mesh_vertices(*mesh, forward_mapping);
        REQUIRE(mesh2->get_num_vertices() == 2);
        REQUIRE(mesh2->get_num_facets() == mesh->get_num_facets());
    }

    SECTION("You can create a discontinuous ordering")
    {
        std::vector<Index> forward_mapping = {3, 3, 2, 2};
        REQUIRE_THROWS(reorder_mesh_vertices(*mesh, forward_mapping));
    }
}

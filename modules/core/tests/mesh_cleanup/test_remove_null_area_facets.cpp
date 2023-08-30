/*
 * Copyright 2023 Adobe. All rights reserved.
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

#include <lagrange/mesh_cleanup/remove_null_area_facets.h>
#include <lagrange/testing/check_mesh.h>

TEST_CASE("remove_null_area_facets", "[mesh_cleanup][surface][degenerate]")
{
    using namespace lagrange;
    using Scalar = float;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0.5f, 0, 0});
    mesh.add_vertex({1, 1e-6f, 0});
    mesh.add_vertex({1, 1e-3f, 0});
    mesh.add_vertex({1, 1e-1f, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_triangle(0, 1, 2); // area: 0
    mesh.add_triangle(0, 1, 3); // area: 1e-6/2
    mesh.add_triangle(0, 1, 4); // area: 1e-3/2
    mesh.add_triangle(0, 1, 5); // area: 1e-1/2
    mesh.add_triangle(0, 1, 6); // area: 1/2

    mesh.initialize_edges();

    RemoveNullAreaFacetsOptions options;
    SECTION("threshold = 0")
    {
        options.null_area_threshold = 0;
        remove_null_area_facets(mesh, options);
        REQUIRE(mesh.get_num_vertices() == 7);
        REQUIRE(mesh.get_num_facets() == 4);
        lagrange::testing::check_mesh(mesh);
    }
    SECTION("threshold = 1e-6")
    {
        options.null_area_threshold = 1e-6f;
        remove_null_area_facets(mesh, options);
        REQUIRE(mesh.get_num_vertices() == 7);
        REQUIRE(mesh.get_num_facets() == 3);
        lagrange::testing::check_mesh(mesh);
    }
    SECTION("threshold = 1e-3")
    {
        options.null_area_threshold = 1e-3f;
        remove_null_area_facets(mesh, options);
        REQUIRE(mesh.get_num_vertices() == 7);
        REQUIRE(mesh.get_num_facets() == 2);
        lagrange::testing::check_mesh(mesh);
    }
    SECTION("threshold = 1e-1")
    {
        options.null_area_threshold = 1e-1f;
        remove_null_area_facets(mesh, options);
        REQUIRE(mesh.get_num_vertices() == 7);
        REQUIRE(mesh.get_num_facets() == 1);
        lagrange::testing::check_mesh(mesh);
    }
    SECTION("threshold = 1")
    {
        options.null_area_threshold = 1;
        remove_null_area_facets(mesh, options);
        REQUIRE(mesh.get_num_vertices() == 7);
        REQUIRE(mesh.get_num_facets() == 0);
        lagrange::testing::check_mesh(mesh);
    }
    SECTION("threshold = 1, remove isolated vertices")
    {
        options.null_area_threshold = 1;
        options.remove_isolated_vertices = true;
        remove_null_area_facets(mesh, options);
        REQUIRE(mesh.get_num_vertices() == 0);
        REQUIRE(mesh.get_num_facets() == 0);
        lagrange::testing::check_mesh(mesh);
    }
}

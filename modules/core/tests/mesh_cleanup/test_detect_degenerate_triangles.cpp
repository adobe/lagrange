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
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/mesh_cleanup/detect_degenerate_triangles.h>

TEST_CASE("DetectDegenerateTrianglesTest", "[degenerate][triangle_mesh]")
{
    using namespace lagrange;
    Vertices3D vertices(3, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;
    Triangles facets(1, 3);
    facets << 0, 1, 2;

    auto mesh = create_mesh(vertices, facets);
    REQUIRE(mesh->get_num_vertices() == 3);
    REQUIRE(mesh->get_num_facets() == 1);

    SECTION("Simple")
    {
        detect_degenerate_triangles(*mesh);
        REQUIRE(mesh->has_facet_attribute("is_degenerate"));

        SECTION("Repeated call")
        {
            detect_degenerate_triangles(*mesh);
            REQUIRE(mesh->has_facet_attribute("is_degenerate"));
        }

        const auto& is_degenerate = mesh->get_facet_attribute("is_degenerate");
        REQUIRE(is_degenerate.rows() == 1);
        REQUIRE(is_degenerate.cols() == 1);
        REQUIRE(is_degenerate(0, 0) == 0.0);
    }

    SECTION("More facets")
    {
        facets.resize(3, 3);
        facets << 0, 1, 2, 0, 1, 1, 2, 2, 2;
        mesh->import_facets(facets);
        detect_degenerate_triangles(*mesh);
        REQUIRE(mesh->has_facet_attribute("is_degenerate"));

        const auto& is_degenerate = mesh->get_facet_attribute("is_degenerate");
        REQUIRE(is_degenerate.rows() == 3);
        REQUIRE(is_degenerate.cols() == 1);
        REQUIRE(is_degenerate(0, 0) == 0.0);
        REQUIRE(is_degenerate(1, 0) == 1.0);
        REQUIRE(is_degenerate(2, 0) == 1.0);
    }
}

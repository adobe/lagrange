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
#include <cmath>

#include <lagrange/Mesh.h>
#include <lagrange/attributes/eval_as_attribute.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>


TEST_CASE("select_vertices", "[select_vertices]")
{
    using namespace lagrange;

    SECTION("rectangle")
    {
        Vertices3D vertices(4, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0;
        Triangles facets(2, 3);
        facets << 0, 1, 2, 2, 1, 3;

        auto mesh = create_mesh(vertices, facets);

        SECTION("cut_through_x")
        {
            auto inout = [](double x, double, double) { return x - 0.5; };
            eval_as_vertex_attribute(*mesh, "is_selected", inout);
            REQUIRE(mesh->has_vertex_attribute("is_selected"));

            const auto& attr = mesh->get_vertex_attribute("is_selected");
            REQUIRE(attr.rows() == mesh->get_num_vertices());
            REQUIRE(attr(0, 0) < 0);
            REQUIRE(attr(1, 0) > 0);
            REQUIRE(attr(2, 0) < 0);
            REQUIRE(attr(3, 0) > 0);
        }

        SECTION("cut_through_diagonal")
        {
            auto inout = [](double x, double y, double) { return x - y; };
            eval_as_vertex_attribute(*mesh, "is_selected", inout);
            REQUIRE(mesh->has_vertex_attribute("is_selected"));

            const auto& attr = mesh->get_vertex_attribute("is_selected");
            REQUIRE(attr.rows() == mesh->get_num_vertices());
            REQUIRE(attr(0, 0) == 0);
            REQUIRE(attr(1, 0) > 0);
            REQUIRE(attr(2, 0) < 0);
            REQUIRE(attr(3, 0) == 0);
        }

        SECTION("missed")
        {
            auto inout = [](double x, double, double) { return x + 0.5; };
            eval_as_vertex_attribute(*mesh, "is_selected", inout);
            REQUIRE(mesh->has_vertex_attribute("is_selected"));

            const auto& attr = mesh->get_vertex_attribute("is_selected");
            REQUIRE(attr.rows() == mesh->get_num_vertices());
            REQUIRE(attr(0, 0) > 0);
            REQUIRE(attr(1, 0) > 0);
            REQUIRE(attr(2, 0) > 0);
            REQUIRE(attr(3, 0) > 0);
        }
    }
}

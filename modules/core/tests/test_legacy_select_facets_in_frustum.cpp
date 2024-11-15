/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
#include <lagrange/testing/common.h>
#include <cmath>

#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/select_facets_in_frustum.h>

template <typename Scalar>
void run_legacy()
{
    using namespace lagrange;

    SECTION("rectangle")
    {
        // 2 +-----+ 3
        //   |\    |
        //   |  \  |
        //   |    \|
        // 0 +-----+ 1
        Vertices3D vertices(4, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0;
        Triangles facets(2, 3);
        facets << 0, 1, 2, 2, 1, 3;

        auto mesh = create_mesh(vertices, facets);
        using MeshType = decltype(mesh)::element_type;
        using VertexType = typename MeshType::VertexType;

        auto select_point = [&](Scalar x, Scalar y, Scalar margin = 0.1) {
            select_facets_in_frustum(
                *mesh,
                VertexType(1, 0, 0),
                VertexType(x - margin, 0, 0),
                VertexType(-1, 0, 0),
                VertexType(x + margin, 0, 0),
                VertexType(0, 1, 0),
                VertexType(0, y - margin, 0),
                VertexType(0, -1, 0),
                VertexType(0, y + margin, 0));
        };

        SECTION("select all")
        {
            select_facets_in_frustum(
                *mesh,
                VertexType(1, 0, 0),
                VertexType(-1, 0, 0),
                VertexType(-1, 0, 0),
                VertexType(2, 0, 0),
                VertexType(0, 1, 0),
                VertexType(0, -1, 0),
                VertexType(0, -1, 0),
                VertexType(0, 2, 0));

            REQUIRE(mesh->has_facet_attribute("is_selected"));

            const auto& attr = mesh->get_facet_attribute("is_selected");
            REQUIRE(attr.rows() == 2);
            REQUIRE(attr(0, 0) > 0);
            REQUIRE(attr(1, 0) > 0);
        }

        SECTION("select none")
        {
            select_facets_in_frustum(
                *mesh,
                VertexType(1, 0, 0),
                VertexType(1.1, 0, 0),
                VertexType(-1, 0, 0),
                VertexType(2, 0, 0),
                VertexType(0, 1, 0),
                VertexType(0, -1, 0),
                VertexType(0, -1, 0),
                VertexType(0, 2, 0));

            REQUIRE(mesh->has_facet_attribute("is_selected"));

            const auto& attr = mesh->get_facet_attribute("is_selected");
            REQUIRE(attr.rows() == 2);
            REQUIRE(attr(0, 0) == 0);
            REQUIRE(attr(1, 0) == 0);
        }

        SECTION("select none again")
        {
            select_facets_in_frustum(
                *mesh,
                VertexType(1, 0, 0),
                VertexType(2, 0, 0),
                VertexType(-1, 0, 0),
                VertexType(-1, 0, 0),
                VertexType(0, 1, 0),
                VertexType(0, 2, 0),
                VertexType(0, -1, 0),
                VertexType(0, -1, 0));

            REQUIRE(mesh->has_facet_attribute("is_selected"));

            const auto& attr = mesh->get_facet_attribute("is_selected");
            REQUIRE(attr.rows() == 2);
            REQUIRE(attr(0, 0) == 0);
            REQUIRE(attr(1, 0) == 0);
        }

        SECTION("select none 3")
        {
            select_facets_in_frustum(
                *mesh,
                VertexType(1, 0, 0),
                VertexType(-1, 0, 0),
                VertexType(-1, 0, 0),
                VertexType(2, 0, 0),
                VertexType(0, 0, 1),
                VertexType(0, 0, 0.5),
                VertexType(0, 0, -1),
                VertexType(0, 0, 1.0));

            REQUIRE(mesh->has_facet_attribute("is_selected"));

            const auto& attr = mesh->get_facet_attribute("is_selected");
            REQUIRE(attr.rows() == 2);
            REQUIRE(attr(0, 0) == 0);
            REQUIRE(attr(1, 0) == 0);
        }

        SECTION("select all again")
        {
            select_facets_in_frustum(
                *mesh,
                VertexType(1, 0, 0),
                VertexType(0.4, 0, 0),
                VertexType(-1, 0, 0),
                VertexType(0.6, 0, 0),
                VertexType(0, 0, 1),
                VertexType(0, 0, -0.1),
                VertexType(0, 0, -1),
                VertexType(0, 0, 0.1));

            REQUIRE(mesh->has_facet_attribute("is_selected"));

            const auto& attr = mesh->get_facet_attribute("is_selected");
            REQUIRE(attr.rows() == 2);
            REQUIRE(attr(0, 0) > 0);
            REQUIRE(attr(1, 0) > 0);
        }

        SECTION("select just the origin")
        {
            select_point(0, 0);
            REQUIRE(mesh->has_facet_attribute("is_selected"));

            const auto& attr = mesh->get_facet_attribute("is_selected");
            REQUIRE(attr.rows() == 2);
            REQUIRE(attr(0, 0) > 0);
            REQUIRE(attr(1, 0) == 0);
        }

        SECTION("select (1, 1)")
        {
            select_point(1, 1);
            REQUIRE(mesh->has_facet_attribute("is_selected"));

            const auto& attr = mesh->get_facet_attribute("is_selected");
            REQUIRE(attr.rows() == 2);
            REQUIRE(attr(0, 0) == 0);
            REQUIRE(attr(1, 0) > 0);
        }

        SECTION("select (0, 1)")
        {
            select_point(0, 1);
            REQUIRE(mesh->has_facet_attribute("is_selected"));

            const auto& attr = mesh->get_facet_attribute("is_selected");
            REQUIRE(attr.rows() == 2);
            REQUIRE(attr(0, 0) > 0);
            REQUIRE(attr(1, 0) > 0);
        }

        SECTION("select (1, 0)")
        {
            select_point(1, 0);
            REQUIRE(mesh->has_facet_attribute("is_selected"));

            const auto& attr = mesh->get_facet_attribute("is_selected");
            REQUIRE(attr.rows() == 2);
            REQUIRE(attr(0, 0) > 0);
            REQUIRE(attr(1, 0) > 0);
        }

        SECTION("select (0.5, 0.5)")
        {
            select_point(0.5, 0.5);
            REQUIRE(mesh->has_facet_attribute("is_selected"));

            const auto& attr = mesh->get_facet_attribute("is_selected");
            REQUIRE(attr.rows() == 2);
            REQUIRE(attr(0, 0) > 0);
            REQUIRE(attr(1, 0) > 0);
        }

        SECTION("select (0.25, 0.25)")
        {
            select_point(0.25, 0.25);
            REQUIRE(mesh->has_facet_attribute("is_selected"));

            const auto& attr = mesh->get_facet_attribute("is_selected");
            REQUIRE(attr.rows() == 2);
            REQUIRE(attr(0, 0) > 0);
            REQUIRE(attr(1, 0) == 0);
        }

        SECTION("select (0.75, 0.75)")
        {
            select_point(0.75, 0.75);
            REQUIRE(mesh->has_facet_attribute("is_selected"));

            const auto& attr = mesh->get_facet_attribute("is_selected");
            REQUIRE(attr.rows() == 2);
            REQUIRE(attr(0, 0) == 0);
            REQUIRE(attr(1, 0) > 0);
        }
    }
}

TEST_CASE("legacy::select_facets_in_frustum<double>", "[select_facets_in_frustum]")
{
    run_legacy<double>();
}
TEST_CASE("legacy::select_facets_in_frustum<float>", "[select_facets_in_frustum]")
{
    run_legacy<float>();
}
#endif

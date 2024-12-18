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

#include <lagrange/mesh_convert.h>
#include <lagrange/select_facets_in_frustum.h>


namespace {

template <typename Scalar>
void run()
{
    using namespace lagrange;
    SECTION("rectangle SurfaceMesh")
    {
        // 2 +-----+ 3
        //   |\    |
        //   |  \  |
        //   |    \|
        // 0 +-----+ 1
        using Index = uint32_t;
        SurfaceMesh<Scalar, Index> surface_mesh;
        surface_mesh.add_vertex({0, 0, 0});
        surface_mesh.add_vertex({1, 0, 0});
        surface_mesh.add_vertex({0, 1, 0});
        surface_mesh.add_vertex({1, 1, 0});
        surface_mesh.add_triangle(0, 1, 2);
        surface_mesh.add_triangle(2, 1, 3);

        auto select_point = [&](Scalar x, Scalar y, Scalar margin = 0.1) {
            // Frustum<Scalar> frustum{
            //     {{
            //       { {{1, 0, 0}}, {{x - margin, 0, 0}} },
            //       { {{-1, 0, 0}}, {{x + margin, 0, 0}} },
            //       { {{0, 1, 0}}, {{0, y - margin, 0}} },
            //       { {{0, -1, 0}}, {{0, y + margin, 0}} }
            //       }}
            //       };
            // Below is equivalent to the above (many) braces initialization
            typename Frustum<Scalar>::Plane plane0{{1, 0, 0}, {x - margin, 0, 0}};
            typename Frustum<Scalar>::Plane plane1{{-1, 0, 0}, {x + margin, 0, 0}};
            typename Frustum<Scalar>::Plane plane2{{0, 1, 0}, {0, y - margin, 0}};
            typename Frustum<Scalar>::Plane plane3{{0, -1, 0}, {0, y + margin, 0}};
            Frustum<Scalar> frustum{{plane0, plane1, plane2, plane3}};

            select_facets_in_frustum(surface_mesh, frustum, FrustumSelectionOptions());
        };

        SECTION("select all")
        {
            Frustum<Scalar> frustum{
                {{{{{1, 0, 0}}, {{-1, 0, 0}}},
                  {{{-1, 0, 0}}, {{2, 0, 0}}},
                  {{{0, 1, 0}}, {{0, -1, 0}}},
                  {{{0, -1, 0}}, {{0, 2, 0}}}}}};

            select_facets_in_frustum(surface_mesh, frustum, FrustumSelectionOptions());

            REQUIRE(surface_mesh.has_attribute("@is_selected"));

            const auto& attr = surface_mesh.template get_attribute<uint8_t>("@is_selected");
            REQUIRE(attr.get_num_elements() == 2); // does this work?
            const auto& attr_ref = attr.get_all();
            REQUIRE(attr_ref[0] > 0);
            REQUIRE(attr_ref[1] > 0);
        }

        SECTION("select none")
        {
            select_facets_in_frustum(
                surface_mesh,
                Frustum<Scalar>{
                    {{{{{1, 0, 0}}, {{1.1f, 0, 0}}},
                      {{{-1, 0, 0}}, {{2, 0, 0}}},
                      {{{0, 1, 0}}, {{0, -1, 0}}},
                      {{{0, -1, 0}}, {{0, 2, 0}}}}}});

            REQUIRE(surface_mesh.has_attribute("@is_selected"));

            const auto& attr = surface_mesh.template get_attribute<uint8_t>("@is_selected");
            REQUIRE(attr.get_num_elements() == 2);
            const auto& attr_ref = attr.get_all();
            REQUIRE(attr_ref[0] == 0);
            REQUIRE(attr_ref[1] == 0);
        }

        SECTION("select none again")
        {
            select_facets_in_frustum(
                surface_mesh,
                Frustum<Scalar>{
                    {{{{{1, 0, 0}}, {{2, 0, 0}}},
                      {{{-1, 0, 0}}, {{-1, 0, 0}}},
                      {{{0, 1, 0}}, {{0, 2, 0}}},
                      {{{0, -1, 0}}, {{0, -1, 0}}}}}});

            REQUIRE(surface_mesh.has_attribute("@is_selected"));

            const auto& attr = surface_mesh.template get_attribute<uint8_t>("@is_selected");
            REQUIRE(attr.get_num_elements() == 2);
            const auto& attr_ref = attr.get_all();
            REQUIRE(attr_ref[0] == 0);
            REQUIRE(attr_ref[1] == 0);
        }

        SECTION("select none 3")
        {
            select_facets_in_frustum(
                surface_mesh,
                Frustum<Scalar>{
                    {{{{{1, 0, 0}}, {{-1, 0, 0}}},
                      {{{-1, 0, 0}}, {{2, 0, 0}}},
                      {{{0, 0, 1}}, {{0, 0, 0.5f}}},
                      {{{0, 0, -1}}, {{0, 0, 1}}}}}});

            REQUIRE(surface_mesh.has_attribute("@is_selected"));

            const auto& attr = surface_mesh.template get_attribute<uint8_t>("@is_selected");
            REQUIRE(attr.get_num_elements() == 2);
            const auto& attr_ref = attr.get_all();
            REQUIRE(attr_ref[0] == 0);
            REQUIRE(attr_ref[1] == 0);
        }

        SECTION("select all again")
        {
            select_facets_in_frustum(
                surface_mesh,
                Frustum<Scalar>{
                    {{{{{1, 0, 0}}, {{0.4f, 0, 0}}},
                      {{{-1, 0, 0}}, {{0.6f, 0, 0}}},
                      {{{0, 0, 1}}, {{0, 0, -0.1f}}},
                      {{{0, 0, -1}}, {{0, 0, 0.1f}}}}}});

            REQUIRE(surface_mesh.has_attribute("@is_selected"));

            const auto& attr = surface_mesh.template get_attribute<uint8_t>("@is_selected");
            REQUIRE(attr.get_num_elements() == 2);
            const auto& attr_ref = attr.get_all();
            REQUIRE(attr_ref[0] > 0);
            REQUIRE(attr_ref[1] > 0);
        }

        SECTION("select just the origin")
        {
            select_point(0, 0);
            REQUIRE(surface_mesh.has_attribute("@is_selected"));

            const auto& attr = surface_mesh.template get_attribute<uint8_t>("@is_selected");
            REQUIRE(attr.get_num_elements() == 2);
            const auto& attr_ref = attr.get_all();
            REQUIRE(attr_ref[0] > 0);
            REQUIRE(attr_ref[1] == 0);
        }

        SECTION("select (1, 1)")
        {
            select_point(1, 1);
            REQUIRE(surface_mesh.has_attribute("@is_selected"));

            const auto& attr = surface_mesh.template get_attribute<uint8_t>("@is_selected");
            REQUIRE(attr.get_num_elements() == 2);
            const auto& attr_ref = attr.get_all();
            REQUIRE(attr_ref[0] == 0);
            REQUIRE(attr_ref[1] > 0);
        }

        SECTION("select (0, 1)")
        {
            select_point(0, 1);
            REQUIRE(surface_mesh.has_attribute("@is_selected"));

            const auto& attr = surface_mesh.template get_attribute<uint8_t>("@is_selected");
            REQUIRE(attr.get_num_elements() == 2);
            const auto& attr_ref = attr.get_all();
            REQUIRE(attr_ref[0] > 0);
            REQUIRE(attr_ref[1] > 0);
        }

        SECTION("select (1, 0)")
        {
            select_point(1, 0);
            REQUIRE(surface_mesh.has_attribute("@is_selected"));

            const auto& attr = surface_mesh.template get_attribute<uint8_t>("@is_selected");
            REQUIRE(attr.get_num_elements() == 2);
            const auto& attr_ref = attr.get_all();
            REQUIRE(attr_ref[0] > 0);
            REQUIRE(attr_ref[1] > 0);
        }

        SECTION("select (0.5, 0.5)")
        {
            select_point(0.5, 0.5);
            REQUIRE(surface_mesh.has_attribute("@is_selected"));

            const auto& attr = surface_mesh.template get_attribute<uint8_t>("@is_selected");
            REQUIRE(attr.get_num_elements() == 2);
            const auto& attr_ref = attr.get_all();
            REQUIRE(attr_ref[0] > 0);
            REQUIRE(attr_ref[1] > 0);
        }

        SECTION("select (0.25, 0.25)")
        {
            select_point(0.25, 0.25);
            REQUIRE(surface_mesh.has_attribute("@is_selected"));

            const auto& attr = surface_mesh.template get_attribute<uint8_t>("@is_selected");
            REQUIRE(attr.get_num_elements() == 2);
            const auto& attr_ref = attr.get_all();
            REQUIRE(attr_ref[0] > 0);
            REQUIRE(attr_ref[1] == 0);
        }

        SECTION("select (0.75, 0.75)")
        {
            select_point(0.75, 0.75);
            REQUIRE(surface_mesh.has_attribute("@is_selected"));

            const auto& attr = surface_mesh.template get_attribute<uint8_t>("@is_selected");
            REQUIRE(attr.get_num_elements() == 2);
            const auto& attr_ref = attr.get_all();
            REQUIRE(attr_ref[0] == 0);
            REQUIRE(attr_ref[1] > 0);
        }
    }
}
} // end namespace

TEST_CASE("select_facets_in_frustum<double>", "[select_facets_in_frustum]")
{
    run<double>();
}
TEST_CASE("select_facets_in_frustum<float>", "[select_facets_in_frustum]")
{
    run<float>();
}

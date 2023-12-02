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

#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/cast.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("cast", "[core][surface][cast]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(2, 1, 3);

    SECTION("geometry only")
    {
        SurfaceMesh<float, uint64_t> mesh2 = cast<float, uint64_t>(mesh);

        REQUIRE(mesh2.get_num_vertices() == 4);
        REQUIRE(mesh2.get_num_facets() == 2);

        auto from_vertices = vertex_view(mesh);
        auto from_facets = facet_view(mesh);
        auto to_vertices = vertex_view(mesh2);
        auto to_facets = facet_view(mesh2);

        REQUIRE_THAT(
            (from_vertices - to_vertices.cast<Scalar>()).norm(),
            Catch::Matchers::WithinAbs(0, 1e-6));
        REQUIRE((from_facets - to_facets.cast<Index>()).cwiseAbs().maxCoeff() == 0);
    }

    SECTION("with uv")
    {
        std::array<Scalar, 4 * 2> uvs{0, 0, 1, 0, 0, 1, 1, 1};
        std::array<Index, 6> uv_indices{0, 1, 2, 2, 1, 3};
        mesh.create_attribute<Scalar>("uv", Indexed, AttributeUsage::UV, 2, uvs, uv_indices);

        SurfaceMesh<float, uint64_t> mesh2 = cast<float, uint64_t>(mesh);
        REQUIRE(mesh2.has_attribute("uv"));

        auto& uv_attr = mesh2.get_indexed_attribute<float>("uv");
        auto& uv_value_attr = uv_attr.values();
        auto& uv_indices_attr = uv_attr.indices();

        REQUIRE(uv_value_attr.get_num_elements() == 4);
        for (Index i = 0; i < 4; i++) {
            REQUIRE_THAT(
                static_cast<Scalar>(uv_value_attr.get(i, 0)),
                Catch::Matchers::WithinAbs(uvs[i * 2], 1e-6));
            REQUIRE_THAT(
                static_cast<Scalar>(uv_value_attr.get(i, 1)),
                Catch::Matchers::WithinAbs(uvs[i * 2 + 1], 1e-6));
        }
        REQUIRE(uv_indices_attr.get_num_elements() == 6);
        for (Index i = 0; i < 6; i++) {
            REQUIRE(static_cast<Index>(uv_indices_attr.get(i)) == uv_indices[i]);
        }
    }
}

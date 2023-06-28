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

#include <lagrange/SurfaceMesh.h>
#include <lagrange/mesh_cleanup/detect_degenerate_facets.h>

#include <catch2/matchers/catch_matchers_vector.hpp>

TEST_CASE("detect_degenerate_facets", "[mesh_cleanup][surface][degenerate]")
{
    using namespace lagrange;
    using Scalar = float;
    using Index = uint32_t;

    SECTION("empty")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        auto degenerate_facets = detect_degenerate_facets(mesh);
        REQUIRE(degenerate_facets.empty());
    }

    SECTION("degenerate")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2); // OK
        mesh.add_triangle(0, 2, 3); // OK
        mesh.add_triangle(0, 0, 3); // Topology degenerate
        mesh.add_triangle(0, 3, 4); // Geometry degenerate
        auto degenerate_facets = detect_degenerate_facets(mesh);
        REQUIRE(degenerate_facets.size() == 2);
        REQUIRE_THAT(
            degenerate_facets,
            Catch::Matchers::UnorderedEquals(std::vector<Index>({2, 3})));
    }

    SECTION("degenerate polygon")
    {
        SurfaceMesh<Scalar, Index> mesh(2);
        mesh.add_vertex({0, 0});
        mesh.add_vertex({1, 0});
        mesh.add_vertex({1, 1});
        mesh.add_vertex({0, 1});
        mesh.add_vertex({0, 1});
        mesh.add_vertex({0, 1});
        mesh.add_quad(0, 1, 2, 3); // OK
        mesh.add_quad(0, 1, 1, 2); // OK
        mesh.add_quad(0, 3, 4, 5); // Geometry degenerate
        auto degenerate_facets = detect_degenerate_facets(mesh);
        REQUIRE(degenerate_facets.size() == 1);
        REQUIRE(degenerate_facets[0] == 2);
    }
}

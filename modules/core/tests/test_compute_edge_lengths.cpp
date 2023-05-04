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
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <lagrange/compute_edge_lengths.h>
#include <lagrange/views.h>

TEST_CASE("compute_edge_lengths", "[core][surface][utilities]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;
    constexpr Scalar eps = std::numeric_limits<Scalar>::epsilon();

    SurfaceMesh<Scalar, Index> mesh;

    SECTION("Triangle")
    {
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);

        auto id = compute_edge_lengths(mesh);
        auto edge_lengths = attribute_matrix_view<Scalar>(mesh, id);
        REQUIRE_THAT(edge_lengths.minCoeff(), Catch::Matchers::WithinAbs(1, eps));
        REQUIRE_THAT(edge_lengths.maxCoeff(), Catch::Matchers::WithinAbs(std::sqrt(2), eps));
    }

    SECTION("Quad")
    {
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_quad(0, 1, 3, 2);

        auto id = compute_edge_lengths(mesh);
        auto edge_lengths = attribute_matrix_view<Scalar>(mesh, id);
        REQUIRE_THAT(edge_lengths.minCoeff(), Catch::Matchers::WithinAbs(1, eps));
        REQUIRE_THAT(edge_lengths.maxCoeff(), Catch::Matchers::WithinAbs(1, eps));
    }
}

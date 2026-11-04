/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <Misha/Geometry.h>
#include <catch2/catch_test_macros.hpp>

#include <vector>

TEST_CASE("Minimal area triangulation caches zero costs", "[texproc][triangulation]")
{
    constexpr unsigned int num_vertices = 16;
    constexpr unsigned int max_evaluations = num_vertices * num_vertices * num_vertices;
    unsigned int evaluations = 0;

    // Collinear boundary vertices have zero cost; bound work instead of timing a potential hang.
    const auto zero_area = [&](unsigned int, unsigned int, unsigned int) {
        REQUIRE(++evaluations <= max_evaluations);
        return 0.;
    };

    SECTION("GetArea")
    {
        const double area = MishaK::MinimalAreaTriangulation::GetArea(zero_area, num_vertices);
        REQUIRE(area == 0.);
    }

    SECTION("GetTriangulation")
    {
        std::vector<MishaK::SimplexIndex<2>> triangles;
        MishaK::MinimalAreaTriangulation::GetTriangulation(zero_area, num_vertices, triangles);
        REQUIRE(triangles.size() == num_vertices - 2);
        for (const auto& triangle : triangles) {
            for (unsigned int i = 0; i < 3; ++i) {
                REQUIRE(static_cast<unsigned int>(triangle[i]) < num_vertices);
            }
        }
    }
}

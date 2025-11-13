/*
 * Copyright 2025 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/geodesic/GeodesicEngineHeat.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("geodesic heat", "[geodesic][heat]")
{
    using Scalar = float;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/ball.obj");

    // Compute geodesic distance using heat method
    auto geodesic_dist_id =
        lagrange::geodesic::make_heat_engine(mesh).single_source_geodesic({}).geodesic_distance_id;

    auto geodesic_distance = lagrange::attribute_vector_view<Scalar>(mesh, geodesic_dist_id);

    REQUIRE_THAT(geodesic_distance.maxCoeff(), Catch::Matchers::WithinRel(M_PI * 10, 0.01));
}

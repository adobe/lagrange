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
#include <lagrange/geodesic/GeodesicEngineDGPC.h>
#include <lagrange/geodesic/GeodesicEngineHeat.h>
#include <lagrange/geodesic/GeodesicEngineMMP.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace {

using Scalar = float;
using Index = uint32_t;

void test_engine(lagrange::geodesic::GeodesicEngine<Scalar, Index>& engine)
{
    SECTION("Same point distance")
    {
        // Distance from a point to itself should be 0
        lagrange::geodesic::PointToPointGeodesicOptions options;
        options.source_facet_id = 0;
        options.target_facet_id = 0;
        options.source_facet_bc = {0.0, 0.0};
        options.target_facet_bc = {0.0, 0.0};

        auto distance = engine.point_to_point_geodesic(options);
        REQUIRE_THAT(distance, Catch::Matchers::WithinAbs(0.0f, 1e-6f));
    }

    SECTION("Different points distance")
    {
        // Distance between two different points should be positive
        lagrange::geodesic::PointToPointGeodesicOptions options;
        options.source_facet_id = 0;
        options.target_facet_id = 10;
        options.source_facet_bc = {0.0, 0.0};
        options.target_facet_bc = {0.0, 0.0};

        auto distance = engine.point_to_point_geodesic(options);
        REQUIRE(distance > 0.0f);
    }

    SECTION("Symmetric distance")
    {
        // Distance from A to B should equal distance from B to A
        lagrange::geodesic::PointToPointGeodesicOptions options1;
        options1.source_facet_id = 5;
        options1.target_facet_id = 15;
        options1.source_facet_bc = {0.3, 0.2};
        options1.target_facet_bc = {0.4, 0.2};

        lagrange::geodesic::PointToPointGeodesicOptions options2;
        options2.source_facet_id = 15;
        options2.target_facet_id = 5;
        options2.source_facet_bc = {0.4, 0.2};
        options2.target_facet_bc = {0.3, 0.2};

        auto distance1 = engine.point_to_point_geodesic(options1);
        auto distance2 = engine.point_to_point_geodesic(options2);

        REQUIRE_THAT(distance1, Catch::Matchers::WithinRel(distance2, 5e-2f));
    }
}

} // namespace

TEST_CASE("compute_geodesic_point_to_point", "[geodesic][point_to_point]")
{
    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/ball.obj");

    SECTION("DGPC Engine")
    {
        auto dgpc_engine = lagrange::geodesic::make_dgpc_engine(mesh);
        test_engine(dgpc_engine);
    }

    SECTION("MMP Engine")
    {
        auto mmp_engine = lagrange::geodesic::make_mmp_engine(mesh);
        test_engine(mmp_engine);
    }

    SECTION("Heat Engine")
    {
        auto heat_engine = lagrange::geodesic::make_heat_engine(mesh);
        test_engine(heat_engine);
    }
}

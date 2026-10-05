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
#include <lagrange/geodesic/GeodesicEngineFlip.h>
#include <lagrange/primitive/generate_icosahedron.h>
#include <lagrange/primitive/generate_subdivided_sphere.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <limits>

namespace {

using Scalar = float;
using Index = uint32_t;

} // namespace

TEST_CASE("geodesic_path_flip", "[geodesic][path][flip]")
{
    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/ball.obj");

    SECTION("Path exists")
    {
        auto engine = lagrange::geodesic::make_flip_engine(mesh);

        lagrange::geodesic::PointToPointGeodesicPathOptions options;
        options.source_facet_id = 0;
        options.target_facet_id = 10;
        options.source_facet_bc = {0.0, 0.0};
        options.target_facet_bc = {0.0, 0.0};

        auto result = engine.point_to_point_geodesic_path(options);

        REQUIRE(!result.points.empty());
        REQUIRE(result.points.size() >= 2);
        REQUIRE(result.facet_ids.size() == result.points.size() - 1);
    }

    SECTION("Path from point to itself")
    {
        auto engine = lagrange::geodesic::make_flip_engine(mesh);

        lagrange::geodesic::PointToPointGeodesicPathOptions options;
        options.source_facet_id = 0;
        options.target_facet_id = 0;
        options.source_facet_bc = {0.0, 0.0};
        options.target_facet_bc = {0.0, 0.0};

        auto result = engine.point_to_point_geodesic_path(options);

        // Same vertex: should return a single point
        REQUIRE(!result.points.empty());
    }

    SECTION("Path endpoints are valid vertices")
    {
        auto engine = lagrange::geodesic::make_flip_engine(mesh);

        lagrange::geodesic::PointToPointGeodesicPathOptions options;
        options.source_facet_id = 5;
        options.target_facet_id = 25;
        options.source_facet_bc = {0.3, 0.3};
        options.target_facet_bc = {0.2, 0.4};

        auto result = engine.point_to_point_geodesic_path(options);
        REQUIRE(!result.points.empty());

        // Verify facet_ids are valid
        for (auto fid : result.facet_ids) {
            REQUIRE(fid < mesh.get_num_facets());
        }
    }

    SECTION("Path length on sphere (north to south pole)")
    {
        // Use subdivided icosphere to ensure a manifold mesh (no duplicate edges at poles).
        lagrange::primitive::SubdividedSphereOptions sphere_options;
        sphere_options.radius = 10.0f;
        sphere_options.subdiv_level = 3;

        auto ico = lagrange::primitive::generate_icosahedron<Scalar, Index>({});
        auto sphere = lagrange::primitive::generate_subdivided_sphere(ico, sphere_options);

        auto engine = lagrange::geodesic::make_flip_engine(sphere);
        auto vertices = lagrange::vertex_view(sphere);
        auto facets = lagrange::facet_view(sphere);

        Scalar radius = static_cast<Scalar>(sphere_options.radius);

        // Find north pole
        Index north_vertex = 0;
        Scalar max_z = std::numeric_limits<Scalar>::lowest();
        for (Index v = 0; v < sphere.get_num_vertices(); ++v) {
            Scalar z = vertices(v, 2);
            if (z > max_z) {
                max_z = z;
                north_vertex = v;
            }
        }

        // Find south pole
        Index south_vertex = 0;
        Scalar min_z = std::numeric_limits<Scalar>::max();
        for (Index v = 0; v < sphere.get_num_vertices(); ++v) {
            Scalar z = vertices(v, 2);
            if (z < min_z) {
                min_z = z;
                south_vertex = v;
            }
        }

        // Find facets containing poles
        Index north_facet = lagrange::invalid<Index>();
        Index south_facet = lagrange::invalid<Index>();
        Index north_lc = 0;
        Index south_lc = 0;

        for (Index f = 0; f < sphere.get_num_facets(); ++f) {
            auto facet = facets.row(f);
            if (north_facet == lagrange::invalid<Index>()) {
                if (facet[0] == north_vertex) {
                    north_facet = f;
                    north_lc = 0;
                } else if (facet[1] == north_vertex) {
                    north_facet = f;
                    north_lc = 1;
                } else if (facet[2] == north_vertex) {
                    north_facet = f;
                    north_lc = 2;
                }
            }
            if (south_facet == lagrange::invalid<Index>()) {
                if (facet[0] == south_vertex) {
                    south_facet = f;
                    south_lc = 0;
                } else if (facet[1] == south_vertex) {
                    south_facet = f;
                    south_lc = 1;
                } else if (facet[2] == south_vertex) {
                    south_facet = f;
                    south_lc = 2;
                }
            }

            if (north_facet != lagrange::invalid<Index>() &&
                south_facet != lagrange::invalid<Index>()) {
                break;
            }
        }

        REQUIRE(north_facet != lagrange::invalid<Index>());
        REQUIRE(south_facet != lagrange::invalid<Index>());

        lagrange::geodesic::PointToPointGeodesicPathOptions options;
        options.source_facet_id = north_facet;
        options.target_facet_id = south_facet;
        options.source_facet_bc = {0.0, 0.0};
        options.target_facet_bc = {0.0, 0.0};
        if (north_lc != 0) {
            options.source_facet_bc[north_lc - 1] = 1;
        }
        if (south_lc != 0) {
            options.target_facet_bc[south_lc - 1] = 1;
        }

        auto result = engine.point_to_point_geodesic_path(options);

        REQUIRE(result.points.size() >= 2);
        REQUIRE(result.facet_ids.size() == result.points.size() - 1);

        // Compute total path length
        Scalar total_length = Scalar(0);
        for (size_t i = 0; i + 1 < result.points.size(); ++i) {
            Scalar dx = result.points[i + 1][0] - result.points[i][0];
            Scalar dy = result.points[i + 1][1] - result.points[i][1];
            Scalar dz = result.points[i + 1][2] - result.points[i][2];
            total_length += std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        // For a sphere, the geodesic distance between antipodal points is pi * radius.
        // Use a 2% tolerance (flip method is approximate, slightly less accurate than MMP).
        Scalar expected_length = static_cast<Scalar>(M_PI) * radius;

        REQUIRE_THAT(total_length, Catch::Matchers::WithinRel(expected_length, 0.02f));

        for (auto fid : result.facet_ids) {
            REQUIRE(fid < sphere.get_num_facets());
        }
    }
}

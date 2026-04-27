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
#include <lagrange/geodesic/GeodesicEngineMMP.h>
#include <lagrange/primitive/generate_sphere.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <limits>

namespace {

using Scalar = float;
using Index = uint32_t;

} // namespace

TEST_CASE("geodesic_path_mmp", "[geodesic][path]")
{
    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/ball.obj");

    SECTION("Path exists")
    {
        // Create MMP engine
        auto engine = lagrange::geodesic::make_mmp_engine(mesh);

        // Compute path between two different points
        lagrange::geodesic::PointToPointGeodesicPathOptions options;
        options.source_facet_id = 0;
        options.target_facet_id = 10;
        options.source_facet_bc = {0.0, 0.0};
        options.target_facet_bc = {0.0, 0.0};

        auto result = engine.point_to_point_geodesic_path(options);

        // Path should not be empty
        REQUIRE(!result.points.empty());

        // Should have at least 2 points (source and target)
        REQUIRE(result.points.size() >= 2);

        // facet_ids should have one fewer entry than points
        REQUIRE(result.facet_ids.size() == result.points.size() - 1);
    }

    SECTION("Path from point to itself")
    {
        auto engine = lagrange::geodesic::make_mmp_engine(mesh);

        lagrange::geodesic::PointToPointGeodesicPathOptions options;
        options.source_facet_id = 0;
        options.target_facet_id = 0;
        options.source_facet_bc = {0.0, 0.0};
        options.target_facet_bc = {0.0, 0.0};

        auto result = engine.point_to_point_geodesic_path(options);

        // Path should still exist but may be trivial (just source/target point)
        REQUIRE(!result.points.empty());
    }

    SECTION("Path endpoints match source/target")
    {
        auto engine = lagrange::geodesic::make_mmp_engine(mesh);

        // Define source and target points
        lagrange::geodesic::PointToPointGeodesicPathOptions options;
        options.source_facet_id = 5;
        options.target_facet_id = 25;
        options.source_facet_bc = {0.3, 0.3};
        options.target_facet_bc = {0.2, 0.4};

        auto result = engine.point_to_point_geodesic_path(options);
        REQUIRE(!result.points.empty());

        // Compute expected source and target positions
        auto mesh_vertices = lagrange::vertex_view(mesh);
        auto mesh_facets = lagrange::facet_view(mesh);

        auto source_facet = mesh_facets.row(options.source_facet_id);
        Eigen::Matrix<Scalar, 3, 1> expected_source =
            (1.0f - options.source_facet_bc[0] - options.source_facet_bc[1]) *
                mesh_vertices.row(source_facet[0]).transpose() +
            options.source_facet_bc[0] * mesh_vertices.row(source_facet[1]).transpose() +
            options.source_facet_bc[1] * mesh_vertices.row(source_facet[2]).transpose();

        auto target_facet = mesh_facets.row(options.target_facet_id);
        Eigen::Matrix<Scalar, 3, 1> expected_target =
            (1.0f - options.target_facet_bc[0] - options.target_facet_bc[1]) *
                mesh_vertices.row(target_facet[0]).transpose() +
            options.target_facet_bc[0] * mesh_vertices.row(target_facet[1]).transpose() +
            options.target_facet_bc[1] * mesh_vertices.row(target_facet[2]).transpose();

        // Check path endpoints
        const auto& src = result.points.front();
        const auto& tgt = result.points.back();
        Eigen::Matrix<Scalar, 3, 1> actual_source(src[0], src[1], src[2]);
        Eigen::Matrix<Scalar, 3, 1> actual_target(tgt[0], tgt[1], tgt[2]);

        Scalar source_error = (actual_source - expected_source).norm();
        Scalar target_error = (actual_target - expected_target).norm();

        REQUIRE_THAT(source_error, Catch::Matchers::WithinAbs(0.0f, 1e-4f));
        REQUIRE_THAT(target_error, Catch::Matchers::WithinAbs(0.0f, 1e-4f));

        // Verify facet_ids are valid
        for (auto fid : result.facet_ids) {
            REQUIRE(fid < mesh.get_num_facets());
        }
    }

    SECTION("Path length on sphere (north to south pole)")
    {
        // Generate a sphere mesh using the primitive module
        lagrange::primitive::SphereOptions sphere_options;
        sphere_options.radius = 10.0f;
        sphere_options.num_longitude_sections = 32;
        sphere_options.num_latitude_sections = 16;
        sphere_options.triangulate = true;

        auto sphere = lagrange::primitive::generate_sphere<Scalar, Index>(sphere_options);

        auto engine = lagrange::geodesic::make_mmp_engine(sphere);
        auto vertices = lagrange::vertex_view(sphere);
        auto facets = lagrange::facet_view(sphere);

        // Get the radius of the sphere
        Scalar radius = static_cast<Scalar>(sphere_options.radius);

        // Find north pole (vertex with maximum z coordinate)
        Index north_vertex = 0;
        Scalar max_z = std::numeric_limits<Scalar>::lowest();
        for (Index v = 0; v < sphere.get_num_vertices(); ++v) {
            Scalar z = vertices(v, 2);
            if (z > max_z) {
                max_z = z;
                north_vertex = v;
            }
        }

        // Find south pole (vertex with minimum z coordinate)
        Index south_vertex = 0;
        Scalar min_z = std::numeric_limits<Scalar>::max();
        for (Index v = 0; v < sphere.get_num_vertices(); ++v) {
            Scalar z = vertices(v, 2);
            if (z < min_z) {
                min_z = z;
                south_vertex = v;
            }
        }

        // Find facets containing the north and south pole vertices
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

        // Compute geodesic path from north to south pole
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

        // For a sphere, the geodesic distance between north and south poles
        // (antipodal points) is exactly pi * radius (half the great circle)
        // The 1% tolerance accounts for mesh discretization.
        Scalar expected_length = static_cast<Scalar>(M_PI) * radius;

        REQUIRE_THAT(total_length, Catch::Matchers::WithinRel(expected_length, 0.01f));

        // Verify all facet_ids are valid
        for (auto fid : result.facet_ids) {
            REQUIRE(fid < sphere.get_num_facets());
        }
    }
}

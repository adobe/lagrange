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

#include <lagrange/polyddg/DifferentialOperators.h>
#include <lagrange/polyddg/compute_principal_curvatures.h>
#include <lagrange/primitive/generate_icosahedron.h>
#include <lagrange/primitive/generate_subdivided_sphere.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>

#include <Eigen/Core>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("compute_principal_curvatures - unit sphere", "[polyddg]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    // Create a subdivided icosphere for uniform triangulation without pole singularities.
    // Level 4 subdivision gives 10242 vertices with excellent uniformity and smoothness.
    // For a unit sphere, both principal curvatures should be 1, and principal directions
    // should be tangent and orthogonal.
    primitive::IcosahedronOptions ico_opts;
    ico_opts.radius = 1.0;
    auto base_icosahedron = primitive::generate_icosahedron<Scalar, Index>(ico_opts);

    primitive::SubdividedSphereOptions subdiv_opts;
    subdiv_opts.radius = 1.0;
    subdiv_opts.subdiv_level = 4;
    auto sphere =
        primitive::generate_subdivided_sphere<Scalar, Index>(base_icosahedron, subdiv_opts);

    polyddg::DifferentialOperators<Scalar, Index> ops(sphere);

    polyddg::PrincipalCurvaturesOptions curvature_opts;
    auto result = polyddg::compute_principal_curvatures(sphere, ops, curvature_opts);

    // Read the computed curvature attributes.
    const auto kappa_min_view = attribute_matrix_view<Scalar>(sphere, result.kappa_min_id);
    const auto kappa_max_view = attribute_matrix_view<Scalar>(sphere, result.kappa_max_id);
    const auto dir_min_view = attribute_matrix_view<Scalar>(sphere, result.direction_min_id);
    const auto dir_max_view = attribute_matrix_view<Scalar>(sphere, result.direction_max_id);

    const Index num_vertices = sphere.get_num_vertices();

    SECTION("principal curvatures near 1 on unit sphere")
    {
        // For a unit sphere with radius 1, both principal curvatures should be 1.
        // Subdivided icosphere has mostly uniform triangulation, though original icosahedron
        // vertices may have slightly larger errors.
        Scalar min_kmin = 1e10, max_kmin = -1e10, sum_kmin = 0;
        Scalar min_kmax = 1e10, max_kmax = -1e10, sum_kmax = 0;
        Index good_vertices = 0;

        for (Index vid = 0; vid < num_vertices; ++vid) {
            Scalar kappa_min = kappa_min_view(vid, 0);
            Scalar kappa_max = kappa_max_view(vid, 0);
            min_kmin = std::min(min_kmin, kappa_min);
            max_kmin = std::max(max_kmin, kappa_min);
            sum_kmin += kappa_min;
            min_kmax = std::min(min_kmax, kappa_max);
            max_kmax = std::max(max_kmax, kappa_max);
            sum_kmax += kappa_max;

            // Count vertices with tight accuracy.
            if (std::abs(kappa_min - 1.0) < 0.05 && std::abs(kappa_max - 1.0) < 0.05) {
                good_vertices++;
            }
        }

        INFO(
            "κ_min range: [" << min_kmin << ", " << max_kmin
                             << "], avg: " << sum_kmin / num_vertices);
        INFO(
            "κ_max range: [" << min_kmax << ", " << max_kmax
                             << "], avg: " << sum_kmax / num_vertices);
        INFO(
            "Vertices within ±0.05: " << good_vertices << " / " << num_vertices << " ("
                                      << (100.0 * good_vertices / num_vertices) << "%)");

        // Check that average curvature is very accurate.
        REQUIRE_THAT(sum_kmin / num_vertices, Catch::Matchers::WithinAbs(1.0, 0.01));
        REQUIRE_THAT(sum_kmax / num_vertices, Catch::Matchers::WithinAbs(1.0, 0.01));

        // Check that vast majority (>99%) of vertices have tight accuracy.
        REQUIRE(good_vertices > static_cast<Index>(0.99 * num_vertices));
    }

    SECTION("principal directions are unit-length")
    {
        for (Index vid = 0; vid < num_vertices; ++vid) {
            Eigen::Matrix<Scalar, 1, 3> dir_min = dir_min_view.row(vid);
            Eigen::Matrix<Scalar, 1, 3> dir_max = dir_max_view.row(vid);
            REQUIRE_THAT(dir_min.norm(), Catch::Matchers::WithinAbs(1.0, 1e-6));
            REQUIRE_THAT(dir_max.norm(), Catch::Matchers::WithinAbs(1.0, 1e-6));
        }
    }

    SECTION("principal directions are tangent to surface")
    {
        // Principal directions should be perpendicular to the vertex normal.
        const auto normal_id = ops.get_vertex_normal_attribute_id();
        const auto normal_view = attribute_matrix_view<Scalar>(sphere, normal_id);

        Scalar max_dot_min = 0, max_dot_max = 0;
        Index worst_vid_min = 0, worst_vid_max = 0;

        for (Index vid = 0; vid < num_vertices; ++vid) {
            Eigen::Matrix<Scalar, 1, 3> dir_min = dir_min_view.row(vid);
            Eigen::Matrix<Scalar, 1, 3> dir_max = dir_max_view.row(vid);
            Eigen::Matrix<Scalar, 1, 3> normal = normal_view.row(vid);

            Scalar dot_min = std::abs(dir_min.dot(normal));
            Scalar dot_max = std::abs(dir_max.dot(normal));
            if (dot_min > max_dot_min) {
                max_dot_min = dot_min;
                worst_vid_min = vid;
            }
            if (dot_max > max_dot_max) {
                max_dot_max = dot_max;
                worst_vid_max = vid;
            }
        }

        INFO("Max |dot(dir_min, normal)|: " << max_dot_min << " at vertex " << worst_vid_min);
        INFO("Max |dot(dir_max, normal)|: " << max_dot_max << " at vertex " << worst_vid_max);

        // Principal directions should be tangent (perpendicular to normal, so dot product ≈ 0).
        // Most vertices should have good tangency; allow for a few outliers.
        REQUIRE(max_dot_min < 0.1);
        REQUIRE(max_dot_max < 0.1);
    }

    SECTION("principal directions are orthogonal")
    {
        // The two principal directions should be perpendicular to each other (dot product ≈ 0).
        Scalar max_dot = 0;
        for (Index vid = 0; vid < num_vertices; ++vid) {
            Eigen::Matrix<Scalar, 1, 3> dir_min = dir_min_view.row(vid);
            Eigen::Matrix<Scalar, 1, 3> dir_max = dir_max_view.row(vid);
            Scalar dot = std::abs(dir_min.dot(dir_max));
            max_dot = std::max(max_dot, dot);
        }

        INFO("Max |dot(dir_min, dir_max)|: " << max_dot);
        // Most vertices should have good orthogonality; allow for a few outliers.
        REQUIRE(max_dot < 0.05);
    }
}

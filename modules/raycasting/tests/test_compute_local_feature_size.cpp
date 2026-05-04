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

#include <lagrange/testing/common.h>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <lagrange/Attribute.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/raycasting/compute_local_feature_size.h>
#include <lagrange/views.h>

TEST_CASE("compute_local_feature_size", "[raycasting][lfs]")
{
    using namespace lagrange;
    using Scalar = float;
    using Index = uint32_t;

    SECTION("empty mesh")
    {
        SurfaceMesh<Scalar, Index> mesh;

        raycasting::LocalFeatureSizeOptions options;
        options.output_attribute_name = "@lfs";
        options.direction_mode = raycasting::RayDirectionMode::Interior;

        auto lfs_id = raycasting::compute_local_feature_size(mesh, options);

        REQUIRE(mesh.has_attribute(options.output_attribute_name));
        const auto& lfs_attr = mesh.get_attribute<Scalar>(lfs_id);
        REQUIRE(lfs_attr.get_num_elements() == 0);
    }

    SECTION("blub")
    {
        auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/blub/blub.obj");

        raycasting::LocalFeatureSizeOptions options;
        options.output_attribute_name = "@lfs";
        options.direction_mode = raycasting::RayDirectionMode::Interior;

        auto lfs_id = raycasting::compute_local_feature_size(mesh, options);

        REQUIRE(mesh.has_attribute(options.output_attribute_name));
        const auto& lfs_attr = mesh.get_attribute<Scalar>(lfs_id);
        REQUIRE(lfs_attr.get_num_elements() == mesh.get_num_vertices());

        auto lfs_values = lagrange::attribute_vector_view<Scalar>(mesh, lfs_id);
        REQUIRE(lfs_values.array().isFinite().all());
        REQUIRE((lfs_values.array() > 0.0f).all());
    }

    SECTION("Bunny mesh")
    {
        auto mesh =
            lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/bunny_simple.obj");

        raycasting::LocalFeatureSizeOptions options;
        options.output_attribute_name = "@lfs";
        options.direction_mode = raycasting::RayDirectionMode::Interior;

        auto lfs_id = raycasting::compute_local_feature_size(mesh, options);

        REQUIRE(mesh.has_attribute(options.output_attribute_name));
        const auto& lfs_attr = mesh.get_attribute<Scalar>(lfs_id);
        REQUIRE(lfs_attr.get_num_elements() == mesh.get_num_vertices());

        auto lfs_values = lagrange::attribute_vector_view<Scalar>(mesh, lfs_id);
        REQUIRE(lfs_values.array().isFinite().all());
        REQUIRE((lfs_values.array() > 0.0f).all());
    }

    SECTION("Different direction modes")
    {
        auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>(
            "open/core/spot/spot_triangulated.obj");

        SECTION("Interior")
        {
            raycasting::LocalFeatureSizeOptions options;
            options.direction_mode = raycasting::RayDirectionMode::Interior;
            options.output_attribute_name = "@lfs_interior";

            auto lfs_id = raycasting::compute_local_feature_size(mesh, options);
            REQUIRE(mesh.has_attribute(options.output_attribute_name));

            const auto& lfs_attr = mesh.get_attribute<Scalar>(lfs_id);
            auto lfs_values = lfs_attr.get_all();

            bool all_finite = true;
            for (auto val : lfs_values) {
                if (!std::isfinite(val)) {
                    all_finite = false;
                    break;
                }
            }
            // With no filtering, all vertices should have finite values
            REQUIRE(all_finite);
        }

        SECTION("Exterior")
        {
            raycasting::LocalFeatureSizeOptions options;
            options.direction_mode = raycasting::RayDirectionMode::Exterior;
            options.output_attribute_name = "@lfs_exterior";

            auto lfs_id = raycasting::compute_local_feature_size(mesh, options);
            REQUIRE(mesh.has_attribute(options.output_attribute_name));

            // For Exterior mode on a closed sphere, rays shoot outward into empty space.
            // We don't expect finite values since there's nothing outside to hit.
            // Just verify the function runs successfully.
            const auto& lfs_attr = mesh.get_attribute<Scalar>(lfs_id);
            REQUIRE(lfs_attr.get_num_elements() == mesh.get_num_vertices());
        }

        SECTION("Both")
        {
            raycasting::LocalFeatureSizeOptions options;
            options.direction_mode = raycasting::RayDirectionMode::Both;
            options.output_attribute_name = "@lfs_both";

            auto lfs_id = raycasting::compute_local_feature_size(mesh, options);
            REQUIRE(mesh.has_attribute(options.output_attribute_name));

            auto lfs_values = lagrange::attribute_vector_view<Scalar>(mesh, lfs_id);
            REQUIRE(lfs_values.array().isFinite().all());
            REQUIRE((lfs_values.array() > 0.0f).all());
        }
    }

    SECTION("Medial axis tolerance")
    {
        auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>(
            "open/core/spot/spot_triangulated.obj");

        SECTION("Loose tolerance")
        {
            raycasting::LocalFeatureSizeOptions options;
            options.output_attribute_name = "@lfs_loose";
            options.medial_axis_tolerance = 1e-3f;
            options.direction_mode = raycasting::RayDirectionMode::Both;

            auto lfs_id = raycasting::compute_local_feature_size(mesh, options);
            REQUIRE(mesh.has_attribute(options.output_attribute_name));

            const auto& lfs_attr = mesh.get_attribute<Scalar>(lfs_id);
            REQUIRE(lfs_attr.get_num_elements() == mesh.get_num_vertices());

            auto lfs_values = lagrange::attribute_vector_view<Scalar>(mesh, lfs_id);
            REQUIRE(lfs_values.array().isFinite().all());
            REQUIRE((lfs_values.array() > 0.0f).all());
        }

        SECTION("Tight tolerance")
        {
            raycasting::LocalFeatureSizeOptions options;
            options.output_attribute_name = "@lfs_tight";
            options.medial_axis_tolerance = 1e-5f;
            options.direction_mode = raycasting::RayDirectionMode::Both;

            auto lfs_id = raycasting::compute_local_feature_size(mesh, options);
            REQUIRE(mesh.has_attribute(options.output_attribute_name));

            auto lfs_values = lagrange::attribute_vector_view<Scalar>(mesh, lfs_id);
            REQUIRE(lfs_values.array().isFinite().all());
            REQUIRE((lfs_values.array() > 0.0f).all());
        }
    }
}

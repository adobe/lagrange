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
#include <lagrange/scene/scene_utils.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("camera matrices", "[scene]")
{
    // clang-format off
    Eigen::Isometry3f world_from_camera = Eigen::Isometry3f::Identity();
    world_from_camera.linear() <<
        0.0, -0.859127402305603, 0.5117617249488831,
        0.0, 0.5117617249488831, 0.859127402305603,
        -1.0, 0.0, 0.0;
    world_from_camera.translation() << 1.535285234451294, 2.5773823261260986, 0.0;
    // clang-format on

    lagrange::scene::Camera camera;
    camera.aspect_ratio = 1.f;
    camera.set_horizontal_fov_from_vertical_fov(0.8726646304130554);
    camera.near_plane = 0.1f;

    SECTION("finite perspective")
    {
        camera.far_plane = 1000.f;

        namespace utils = lagrange::scene::utils;
        auto view_transform = utils::camera_view_transform(camera, world_from_camera);
        auto proj_transform = utils::camera_projection_transform(camera);

        // clang-format off
        Eigen::Matrix4f expected_view;
        expected_view <<
                    0x0p+0,          0x0p+0,         -0x1p+0,          0x0p+0,
            -0x1.b7df8cp-1,   0x1.0605a2p-1,          0x0p+0,  -0x1.50aeep-27,
             0x1.0605a2p-1,   0x1.b7df8cp-1,          0x0p+0,     -0x1.8p+1,
                    0x0p+0,          0x0p+0,          0x0p+0,          0x1p+0;
        Eigen::Matrix4f expected_proj;
        expected_proj <<
             0x1.127f34p+1,         0x0p+0,         0x0p+0,         0x0p+0,
                    0x0p+0,  0x1.127f34p+1,         0x0p+0,         0x0p+0,
                    0x0p+0,         0x0p+0, -0x1.000d1cp+0, -0x1.99a416p-3,
                    0x0p+0,         0x0p+0,        -0x1p+0,         0x0p+0;
        // clang-format on

        REQUIRE(view_transform.matrix() == expected_view);
        REQUIRE(proj_transform.matrix() == expected_proj);
    }

    SECTION("infinite perspective")
    {
        camera.far_plane = std::nullopt;

        namespace utils = lagrange::scene::utils;
        auto view_transform = utils::camera_view_transform(camera, world_from_camera);
        auto proj_transform = utils::camera_projection_transform(camera);

        // clang-format off
        Eigen::Matrix4f expected_view;
        expected_view <<
                    0x0p+0,          0x0p+0,         -0x1p+0,          0x0p+0,
            -0x1.b7df8cp-1,   0x1.0605a2p-1,          0x0p+0,  -0x1.50aeep-27,
             0x1.0605a2p-1,   0x1.b7df8cp-1,          0x0p+0,     -0x1.8p+1,
                    0x0p+0,          0x0p+0,          0x0p+0,          0x1p+0;
        Eigen::Matrix4f expected_proj;
        expected_proj <<
             0x1.127f34p+1,         0x0p+0,         0x0p+0,         0x0p+0,
                    0x0p+0,  0x1.127f34p+1,         0x0p+0,         0x0p+0,
                    0x0p+0,         0x0p+0,        -0x1p+0, -0x1.99999ap-3,
                    0x0p+0,         0x0p+0,        -0x1p+0,         0x0p+0;
        // clang-format on

        REQUIRE(view_transform.matrix() == expected_view);
        REQUIRE(proj_transform.matrix() == expected_proj);
    }

    SECTION("orthographic")
    {
        camera.type = lagrange::scene::Camera::Type::Orthographic;
        camera.orthographic_width = 1.f;

        namespace utils = lagrange::scene::utils;
        auto view_transform = utils::camera_view_transform(camera, world_from_camera);
        auto proj_transform = utils::camera_projection_transform(camera);

        // clang-format off
        Eigen::Matrix4f expected_view;
        expected_view <<
                    0x0p+0,          0x0p+0,         -0x1p+0,          0x0p+0,
            -0x1.b7df8cp-1,   0x1.0605a2p-1,          0x0p+0,  -0x1.50aeep-27,
             0x1.0605a2p-1,   0x1.b7df8cp-1,          0x0p+0,     -0x1.8p+1,
                    0x0p+0,          0x0p+0,          0x0p+0,          0x1p+0;
        Eigen::Matrix4f expected_proj;
        expected_proj <<
                    0x1p+0,         0x0p+0,         0x0p+0,        -0x0p+0,
                    0x0p+0,         0x1p+0,         0x0p+0,        -0x0p+0,
                    0x0p+0,         0x0p+0, -0x1.062b94p-9, -0x1.000d1cp+0,
                    0x0p+0,         0x0p+0,         0x0p+0,         0x1p+0;
        // clang-format on

        REQUIRE(view_transform.matrix() == expected_view);
        REQUIRE(proj_transform.matrix() == expected_proj);
    }

    SECTION("affine view transform without scale")
    {
        camera.far_plane = 1000.f;

        // Convert the Isometry3f to Affine3f (no scale added)
        Eigen::Affine3f world_affine = Eigen::Affine3f::Identity();
        world_affine.linear() = world_from_camera.linear();
        world_affine.translation() = world_from_camera.translation();

        namespace utils = lagrange::scene::utils;
        auto view_isometry = utils::camera_view_transform(camera, world_from_camera);
        auto view_affine = utils::camera_view_transform(camera, world_affine);

        REQUIRE(view_affine.matrix().isApprox(view_isometry.matrix()));
    }

    SECTION("affine view transform with uniform scale")
    {
        camera.far_plane = 1000.f;

        // Add a uniform scale factor (e.g. Blender's unit conversion)
        Eigen::Affine3f world_scaled = Eigen::Affine3f::Identity();
        world_scaled.linear() = 0.403f * world_from_camera.linear();
        world_scaled.translation() = world_from_camera.translation();

        namespace utils = lagrange::scene::utils;
        auto view_isometry = utils::camera_view_transform(camera, world_from_camera);
        auto view_scaled = utils::camera_view_transform(camera, world_scaled);

        // Scale should be stripped per glTF §3.10.2, so results must match
        REQUIRE(view_scaled.matrix().isApprox(view_isometry.matrix()));
    }

    SECTION("affine view transform with non-uniform scale")
    {
        camera.far_plane = 1000.f;

        // Non-uniform scale on each axis
        Eigen::Affine3f world_scaled = Eigen::Affine3f::Identity();
        world_scaled.linear() = world_from_camera.linear();
        world_scaled.linear().col(0) *= 2.0f;
        world_scaled.linear().col(1) *= 0.5f;
        world_scaled.linear().col(2) *= 3.0f;
        world_scaled.translation() = world_from_camera.translation();

        namespace utils = lagrange::scene::utils;
        auto view_isometry = utils::camera_view_transform(camera, world_from_camera);
        auto view_scaled = utils::camera_view_transform(camera, world_scaled);

        // Scale should be stripped per glTF §3.10.2, so results must match
        REQUIRE(view_scaled.matrix().isApprox(view_isometry.matrix()));
    }
}

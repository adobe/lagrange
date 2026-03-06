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
#include <lagrange/create_mesh.h>
#include <lagrange/internal/constants.h>
#include <lagrange/mesh_convert.h>
#include <lagrange/raycasting/RayCaster.h>
#include <lagrange/utils/BitField.h>
#include <lagrange/utils/range.h>

#include <lagrange/testing/common.h>
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <catch2/catch_approx.hpp>

namespace {

/// Create a SurfaceMesh unit cube from the legacy create_cube().
lagrange::SurfaceMesh32d create_cube_surface_mesh()
{
    return lagrange::to_surface_mesh_copy<double, uint32_t>(*lagrange::create_cube());
}

} // namespace

TEST_CASE("RayCaster: single ray", "[raycasting][RayCaster]")
{
    namespace rc = lagrange::raycasting;

    auto cube = create_cube_surface_mesh();

    rc::RayCaster caster(rc::SceneFlags::Robust, rc::BuildQuality::High);
    caster.add_mesh(std::move(cube));
    caster.commit_updates();

    SECTION("hit from outside")
    {
        // Ray from (5,0,0) toward (-1,0,0) should hit the cube at x=1
        auto hit = caster.cast(Eigen::Vector3f(5, 0, 0), Eigen::Vector3f(-1, 0, 0));
        REQUIRE(hit.has_value());
        REQUIRE(hit->mesh_index == 0);
        REQUIRE(hit->ray_depth == Catch::Approx(4.0f).margin(1e-4f));
        REQUIRE(hit->position.x() == Catch::Approx(1.0f).margin(1e-4f));
    }

    SECTION("miss")
    {
        // Ray from (5,0,0) toward (1,0,0) should miss (going away from cube)
        auto hit = caster.cast(Eigen::Vector3f(5, 0, 0), Eigen::Vector3f(1, 0, 0));
        REQUIRE(!hit.has_value());
    }

    SECTION("hit from inside")
    {
        // Ray from origin toward (1,0,0) should hit the cube at x=1
        auto hit = caster.cast(Eigen::Vector3f(0, 0, 0), Eigen::Vector3f(1, 0, 0));
        REQUIRE(hit.has_value());
        REQUIRE(hit->mesh_index == 0);
        REQUIRE(hit->position.x() == Catch::Approx(1.0f).margin(1e-4f));
    }

    SECTION("all directions hit from inside")
    {
        // From the center, rays in all spherical directions should hit the cube
        const int order = 10;
        int hit_count = 0;
        int total = 0;
        for (int i = 0; i <= order; ++i) {
            float theta = float(i) / float(order) * 2.f * float(lagrange::internal::pi);
            for (int j = 0; j <= order; ++j) {
                float phi = float(j) / float(order) * float(lagrange::internal::pi) -
                            float(lagrange::internal::pi) * 0.5f;
                Eigen::Vector3f dir(
                    std::cos(phi) * std::cos(theta),
                    std::cos(phi) * std::sin(theta),
                    std::sin(phi));

                auto hit = caster.cast(Eigen::Vector3f(0, 0, 0), dir);
                if (hit.has_value()) {
                    hit_count++;
                }
                total++;
            }
        }
        REQUIRE(hit_count == total);
    }
}

TEST_CASE("RayCaster: occluded", "[raycasting][RayCaster]")
{
    namespace rc = lagrange::raycasting;

    auto cube = create_cube_surface_mesh();

    rc::RayCaster caster(rc::SceneFlags::Robust, rc::BuildQuality::High);
    caster.add_mesh(std::move(cube));
    caster.commit_updates();

    SECTION("occluded from outside")
    {
        bool occ = caster.occluded(Eigen::Vector3f(5, 0, 0), Eigen::Vector3f(-1, 0, 0));
        REQUIRE(occ);
    }

    SECTION("not occluded")
    {
        bool occ = caster.occluded(Eigen::Vector3f(5, 0, 0), Eigen::Vector3f(1, 0, 0));
        REQUIRE(!occ);
    }
}

TEST_CASE("RayCaster: closest_point", "[raycasting][RayCaster]")
{
    namespace rc = lagrange::raycasting;

    auto cube = create_cube_surface_mesh();

    rc::RayCaster caster(rc::SceneFlags::Robust, rc::BuildQuality::High);
    caster.add_mesh(std::move(cube));
    caster.commit_updates();

    SECTION("point on face")
    {
        // Query from (2,0,0): closest point should be on the +x face at (1,0,0)
        auto hit = caster.closest_point(Eigen::Vector3f(2, 0, 0));
        REQUIRE(hit.has_value());
        REQUIRE(hit->position.x() == Catch::Approx(1.0f).margin(1e-4f));
        REQUIRE(hit->position.y() == Catch::Approx(0.0f).margin(1e-4f));
        REQUIRE(hit->position.z() == Catch::Approx(0.0f).margin(1e-4f));
        REQUIRE(hit->mesh_index == 0);
    }

    SECTION("point on edge")
    {
        // Query from (2,2,0): closest point should be on the edge at (1,1,0)
        auto hit = caster.closest_point(Eigen::Vector3f(2, 2, 0));
        REQUIRE(hit.has_value());
        REQUIRE(hit->position.x() == Catch::Approx(1.0f).margin(1e-4f));
        REQUIRE(hit->position.y() == Catch::Approx(1.0f).margin(1e-4f));
        REQUIRE(hit->position.z() == Catch::Approx(0.0f).margin(1e-4f));
    }

    SECTION("point on vertex")
    {
        // Query from (2,2,2): closest point should be at (1,1,1)
        auto hit = caster.closest_point(Eigen::Vector3f(2, 2, 2));
        REQUIRE(hit.has_value());
        REQUIRE(hit->position.x() == Catch::Approx(1.0f).margin(1e-4f));
        REQUIRE(hit->position.y() == Catch::Approx(1.0f).margin(1e-4f));
        REQUIRE(hit->position.z() == Catch::Approx(1.0f).margin(1e-4f));
    }
}

TEST_CASE("RayCaster: cast4 packet", "[raycasting][RayCaster]")
{
    namespace rc = lagrange::raycasting;

    auto cube = create_cube_surface_mesh();

    rc::RayCaster caster(rc::SceneFlags::Robust, rc::BuildQuality::High);
    caster.add_mesh(std::move(cube));
    caster.commit_updates();

    SECTION("all 4 rays hit")
    {
        rc::RayCaster::Point4f origins = rc::RayCaster::Point4f::Zero();
        rc::RayCaster::Direction4f dirs = rc::RayCaster::Direction4f::Zero();
        for (int i = 0; i < 4; ++i) {
            origins(i, 0) = 5.0f;
            dirs(i, 0) = -1.0f;
        }

        auto result = caster.cast4(origins, dirs, size_t(4));
        for (int i = 0; i < 4; ++i) {
            REQUIRE(result.is_valid(i));
            REQUIRE(result.ray_depths(i) == Catch::Approx(4.0f).margin(1e-3f));
        }
    }

    SECTION("partial mask")
    {
        rc::RayCaster::Point4f origins = rc::RayCaster::Point4f::Zero();
        rc::RayCaster::Direction4f dirs = rc::RayCaster::Direction4f::Zero();
        for (int i = 0; i < 4; ++i) {
            origins(i, 0) = 5.0f;
            dirs(i, 0) = -1.0f;
        }

        // Only activate first 2 rays
        auto result = caster.cast4(origins, dirs, size_t(2));
        for (int i = 0; i < 2; ++i) {
            REQUIRE(result.is_valid(i));
        }
    }

    SECTION("mixed hit/miss")
    {
        rc::RayCaster::Point4f origins = rc::RayCaster::Point4f::Zero();
        rc::RayCaster::Direction4f dirs = rc::RayCaster::Direction4f::Zero();

        for (int i = 0; i < 4; ++i) {
            origins(i, 0) = 5.0f;
            if (i % 2 == 0) {
                dirs(i, 0) = -1.0f; // toward cube
            } else {
                dirs(i, 0) = 1.0f; // away from cube
            }
        }

        auto result = caster.cast4(origins, dirs, size_t(4));
        for (int i = 0; i < 4; ++i) {
            if (i % 2 == 0) {
                REQUIRE(result.is_valid(i));
            } else {
                REQUIRE(!result.is_valid(i));
            }
        }
    }
}

TEST_CASE("RayCaster: cast8 packet", "[raycasting][RayCaster]")
{
    namespace rc = lagrange::raycasting;

    auto cube = create_cube_surface_mesh();

    rc::RayCaster caster(rc::SceneFlags::Robust, rc::BuildQuality::High);
    caster.add_mesh(std::move(cube));
    caster.commit_updates();

    SECTION("all 8 rays hit")
    {
        rc::RayCaster::Point8f origins = rc::RayCaster::Point8f::Zero();
        rc::RayCaster::Direction8f dirs = rc::RayCaster::Direction8f::Zero();
        for (int i = 0; i < 8; ++i) {
            origins(i, 0) = 5.0f;
            dirs(i, 0) = -1.0f;
        }

        auto result = caster.cast8(origins, dirs, size_t(8));
        for (int i = 0; i < 8; ++i) {
            REQUIRE(result.is_valid(i));
            REQUIRE(result.ray_depths(i) == Catch::Approx(4.0f).margin(1e-3f));
        }
    }

    SECTION("partial mask")
    {
        rc::RayCaster::Point8f origins = rc::RayCaster::Point8f::Zero();
        rc::RayCaster::Direction8f dirs = rc::RayCaster::Direction8f::Zero();
        for (int i = 0; i < 8; ++i) {
            origins(i, 0) = 5.0f;
            dirs(i, 0) = -1.0f;
        }

        // Only activate first 3 rays
        auto result = caster.cast8(origins, dirs, size_t(3));
        for (int i = 0; i < 3; ++i) {
            REQUIRE(result.is_valid(i));
        }
    }

    SECTION("mixed hit/miss")
    {
        rc::RayCaster::Point8f origins = rc::RayCaster::Point8f::Zero();
        rc::RayCaster::Direction8f dirs = rc::RayCaster::Direction8f::Zero();

        for (int i = 0; i < 8; ++i) {
            origins(i, 0) = 5.0f;
            if (i % 2 == 0) {
                dirs(i, 0) = -1.0f; // toward cube
            } else {
                dirs(i, 0) = 1.0f; // away from cube
            }
        }

        auto result = caster.cast8(origins, dirs, size_t(8));
        for (int i = 0; i < 8; ++i) {
            if (i % 2 == 0) {
                REQUIRE(result.is_valid(i));
            } else {
                REQUIRE(!result.is_valid(i));
            }
        }
    }
}

TEST_CASE("RayCaster: cast16 packet", "[raycasting][RayCaster]")
{
    namespace rc = lagrange::raycasting;

    auto cube = create_cube_surface_mesh();

    rc::RayCaster caster(rc::SceneFlags::Robust, rc::BuildQuality::High);
    caster.add_mesh(std::move(cube));
    caster.commit_updates();

    SECTION("all 16 rays hit")
    {
        rc::RayCaster::Point16f origins = rc::RayCaster::Point16f::Zero();
        rc::RayCaster::Direction16f dirs = rc::RayCaster::Direction16f::Zero();
        for (int i = 0; i < 16; ++i) {
            origins(i, 0) = 5.0f;
            origins(i, 1) = 0.0f;
            origins(i, 2) = 0.0f;
            dirs(i, 0) = -1.0f;
        }

        auto result = caster.cast16(origins, dirs, size_t(16));
        for (int i = 0; i < 16; ++i) {
            REQUIRE(result.is_valid(i));
            REQUIRE(result.ray_depths(i) == Catch::Approx(4.0f).margin(1e-3f));
        }
    }

    SECTION("partial mask")
    {
        rc::RayCaster::Point16f origins = rc::RayCaster::Point16f::Zero();
        rc::RayCaster::Direction16f dirs = rc::RayCaster::Direction16f::Zero();
        for (int i = 0; i < 16; ++i) {
            origins(i, 0) = 5.0f;
            dirs(i, 0) = -1.0f;
        }

        // Only activate first 4 rays
        auto result = caster.cast16(origins, dirs, size_t(4));
        for (int i = 0; i < 4; ++i) {
            REQUIRE(result.is_valid(i));
        }
    }

    SECTION("mixed hit/miss")
    {
        rc::RayCaster::Point16f origins = rc::RayCaster::Point16f::Zero();
        rc::RayCaster::Direction16f dirs = rc::RayCaster::Direction16f::Zero();

        // Even rays hit, odd rays miss
        for (int i = 0; i < 16; ++i) {
            origins(i, 0) = 5.0f;
            if (i % 2 == 0) {
                dirs(i, 0) = -1.0f; // toward cube
            } else {
                dirs(i, 0) = 1.0f; // away from cube
            }
        }

        auto result = caster.cast16(origins, dirs, size_t(16));
        for (int i = 0; i < 16; ++i) {
            if (i % 2 == 0) {
                REQUIRE(result.is_valid(i));
            } else {
                REQUIRE(!result.is_valid(i));
            }
        }
    }
}

TEST_CASE("RayCaster: closest_point16 packet", "[raycasting][RayCaster]")
{
    namespace rc = lagrange::raycasting;

    auto cube = create_cube_surface_mesh();

    rc::RayCaster caster(rc::SceneFlags::Robust, rc::BuildQuality::High);
    caster.add_mesh(std::move(cube));
    caster.commit_updates();

    rc::RayCaster::Point16f queries = rc::RayCaster::Point16f::Zero();
    for (int i = 0; i < 16; ++i) {
        queries(i, 0) = 2.0f; // All query points at (2,0,0)
    }

    auto result = caster.closest_point16(queries, size_t(16));
    for (int i = 0; i < 16; ++i) {
        REQUIRE(result.is_valid(i));
        REQUIRE(result.positions(0, i) == Catch::Approx(1.0f).margin(1e-4f));
        REQUIRE(result.positions(1, i) == Catch::Approx(0.0f).margin(1e-4f));
        REQUIRE(result.positions(2, i) == Catch::Approx(0.0f).margin(1e-4f));
    }
}

TEST_CASE("RayCaster: multiple meshes", "[raycasting][RayCaster]")
{
    namespace rc = lagrange::raycasting;

    auto cube1 = create_cube_surface_mesh();
    auto cube2 = create_cube_surface_mesh();

    rc::RayCaster caster(rc::SceneFlags::Robust, rc::BuildQuality::High);

    // Place cube1 at identity (centered at origin, extent [-1,1]^3)
    auto mesh_id1 = caster.add_mesh(std::move(cube1));

    // Place cube2 translated to (10,0,0)
    Eigen::Transform<double, 3, Eigen::Affine> t2 =
        Eigen::Transform<double, 3, Eigen::Affine>::Identity();
    t2.translate(Eigen::Vector3d(10, 0, 0));
    auto mesh_id2 = caster.add_mesh(std::move(cube2), std::make_optional(t2));

    caster.commit_updates();

    SECTION("hit first cube")
    {
        auto hit = caster.cast(Eigen::Vector3f(-5, 0, 0), Eigen::Vector3f(1, 0, 0));
        REQUIRE(hit.has_value());
        REQUIRE(hit->mesh_index == mesh_id1);
    }

    SECTION("hit second cube")
    {
        auto hit = caster.cast(Eigen::Vector3f(15, 0, 0), Eigen::Vector3f(-1, 0, 0));
        REQUIRE(hit.has_value());
        REQUIRE(hit->mesh_index == mesh_id2);
        REQUIRE(hit->position.x() == Catch::Approx(11.0f).margin(1e-3f));
    }
}

TEST_CASE("RayCaster: multiple instances", "[raycasting][RayCaster]")
{
    namespace rc = lagrange::raycasting;

    auto cube = create_cube_surface_mesh();

    rc::RayCaster caster(rc::SceneFlags::Robust, rc::BuildQuality::High);

    // Add a single mesh with identity transform
    auto mesh_id = caster.add_mesh(std::move(cube));

    // Add a second instance translated to (10,0,0)
    Eigen::Affine3f t2 = Eigen::Affine3f::Identity();
    t2.translate(Eigen::Vector3f(10, 0, 0));
    auto instance_id = caster.add_instance(mesh_id, t2);

    caster.commit_updates();

    SECTION("hit original instance")
    {
        auto hit = caster.cast(Eigen::Vector3f(-5, 0, 0), Eigen::Vector3f(1, 0, 0));
        REQUIRE(hit.has_value());
        REQUIRE(hit->mesh_index == mesh_id);
        REQUIRE(hit->instance_index == 0);
    }

    SECTION("hit second instance")
    {
        auto hit = caster.cast(Eigen::Vector3f(15, 0, 0), Eigen::Vector3f(-1, 0, 0));
        REQUIRE(hit.has_value());
        REQUIRE(hit->mesh_index == mesh_id);
        REQUIRE(hit->instance_index == instance_id);
    }

    SECTION("get transform")
    {
        auto tr = caster.get_transform(mesh_id, instance_id);
        REQUIRE(tr.translation().x() == Catch::Approx(10.0f).margin(1e-6f));
    }
}

TEST_CASE("RayCaster: visibility", "[raycasting][RayCaster]")
{
    namespace rc = lagrange::raycasting;

    auto cube = create_cube_surface_mesh();

    rc::RayCaster caster(rc::SceneFlags::Robust, rc::BuildQuality::High);
    auto mesh_id = caster.add_mesh(std::move(cube));
    caster.commit_updates();

    SECTION("visible by default")
    {
        REQUIRE(caster.get_visibility(mesh_id, 0));
        auto hit = caster.cast(Eigen::Vector3f(5, 0, 0), Eigen::Vector3f(-1, 0, 0));
        REQUIRE(hit.has_value());
    }

    SECTION("hidden instance")
    {
        caster.update_visibility(mesh_id, 0, false);
        caster.commit_updates();

        REQUIRE(!caster.get_visibility(mesh_id, 0));
        auto hit = caster.cast(Eigen::Vector3f(5, 0, 0), Eigen::Vector3f(-1, 0, 0));
        REQUIRE(!hit.has_value());
    }

    SECTION("re-show instance")
    {
        caster.update_visibility(mesh_id, 0, false);
        caster.commit_updates();
        REQUIRE(!caster.get_visibility(mesh_id, 0));

        caster.update_visibility(mesh_id, 0, true);
        caster.commit_updates();
        REQUIRE(caster.get_visibility(mesh_id, 0));

        auto hit = caster.cast(Eigen::Vector3f(5, 0, 0), Eigen::Vector3f(-1, 0, 0));
        REQUIRE(hit.has_value());
    }
}

TEST_CASE("RayCaster: update_vertices", "[raycasting][RayCaster]")
{
    namespace rc = lagrange::raycasting;

    auto cube = create_cube_surface_mesh();

    rc::RayCaster caster(rc::SceneFlags::Robust, rc::BuildQuality::High);
    auto mesh_id = caster.add_mesh(std::move(cube));
    caster.commit_updates();

    // Initially should hit
    auto hit = caster.cast(Eigen::Vector3f(5, 0, 0), Eigen::Vector3f(-1, 0, 0));
    REQUIRE(hit.has_value());
    REQUIRE(hit->position.x() == Catch::Approx(1.0f).margin(1e-4f));

    // Update mesh: scale cube by 2x
    auto scaled_cube = create_cube_surface_mesh();
    {
        auto V = lagrange::vertex_ref(scaled_cube);
        V *= 2.0;
    }
    caster.update_vertices(mesh_id, scaled_cube);
    caster.commit_updates();

    // Now should hit at x=2
    hit = caster.cast(Eigen::Vector3f(5, 0, 0), Eigen::Vector3f(-1, 0, 0));
    REQUIRE(hit.has_value());
    REQUIRE(hit->position.x() == Catch::Approx(2.0f).margin(1e-3f));
}

TEST_CASE("RayCaster: intersection filter", "[raycasting][RayCaster]")
{
    namespace rc = lagrange::raycasting;

    auto cube = create_cube_surface_mesh();

    // Filters require SceneFlags::Filter to be set.
    rc::RayCaster caster(
        lagrange::BitField(rc::SceneFlags::Robust) | lagrange::BitField(rc::SceneFlags::Filter),
        rc::BuildQuality::High);
    auto mesh_id = caster.add_mesh(std::move(cube));
    caster.commit_updates();

    // Without filter, should hit
    auto hit = caster.cast(Eigen::Vector3f(5, 0, 0), Eigen::Vector3f(-1, 0, 0));
    REQUIRE(hit.has_value());

    // Set a filter that rejects all hits
    caster.set_intersection_filter(
        mesh_id,
        [](uint32_t /*instance_index*/, uint32_t /*facet_index*/) { return false; });

    hit = caster.cast(Eigen::Vector3f(5, 0, 0), Eigen::Vector3f(-1, 0, 0));
    REQUIRE(!hit.has_value());

    // Remove filter
    caster.set_intersection_filter(mesh_id, {});

    hit = caster.cast(Eigen::Vector3f(5, 0, 0), Eigen::Vector3f(-1, 0, 0));
    REQUIRE(hit.has_value());
}

TEST_CASE("RayCaster: occlusion filter", "[raycasting][RayCaster]")
{
    namespace rc = lagrange::raycasting;

    auto cube = create_cube_surface_mesh();

    // Filters require SceneFlags::Filter to be set.
    rc::RayCaster caster(
        lagrange::BitField(rc::SceneFlags::Robust) | lagrange::BitField(rc::SceneFlags::Filter),
        rc::BuildQuality::High);
    auto mesh_id = caster.add_mesh(std::move(cube));
    caster.commit_updates();

    // Without filter, should be occluded
    REQUIRE(caster.occluded(Eigen::Vector3f(5, 0, 0), Eigen::Vector3f(-1, 0, 0)));

    // Set filter that rejects all hits
    caster.set_occlusion_filter(mesh_id, [](uint32_t /*instance_index*/, uint32_t /*facet_index*/) {
        return false;
    });

    REQUIRE(!caster.occluded(Eigen::Vector3f(5, 0, 0), Eigen::Vector3f(-1, 0, 0)));

    // Remove filter
    caster.set_occlusion_filter(mesh_id, {});

    REQUIRE(caster.occluded(Eigen::Vector3f(5, 0, 0), Eigen::Vector3f(-1, 0, 0)));
}

TEST_CASE("RayCaster: update_transform", "[raycasting][RayCaster]")
{
    namespace rc = lagrange::raycasting;

    auto cube = create_cube_surface_mesh();

    rc::RayCaster caster(rc::SceneFlags::Robust, rc::BuildQuality::High);
    auto mesh_id = caster.add_mesh(std::move(cube));
    caster.commit_updates();

    // Initially should hit at x=1
    auto hit = caster.cast(Eigen::Vector3f(5, 0, 0), Eigen::Vector3f(-1, 0, 0));
    REQUIRE(hit.has_value());
    REQUIRE(hit->position.x() == Catch::Approx(1.0f).margin(1e-4f));

    // Move cube to (10,0,0)
    Eigen::Affine3f t = Eigen::Affine3f::Identity();
    t.translate(Eigen::Vector3f(10, 0, 0));
    caster.update_transform(mesh_id, 0, t);
    caster.commit_updates();

    // Old position should miss
    hit = caster.cast(Eigen::Vector3f(5, 0, 0), Eigen::Vector3f(-1, 0, 0));
    REQUIRE(!hit.has_value());

    // New position should hit at x=9 (10-1)
    hit = caster.cast(Eigen::Vector3f(15, 0, 0), Eigen::Vector3f(-1, 0, 0));
    REQUIRE(hit.has_value());
    REQUIRE(hit->position.x() == Catch::Approx(11.0f).margin(1e-3f));
}

TEST_CASE("RayCaster: rotated directions", "[raycasting][RayCaster]")
{
    namespace rc = lagrange::raycasting;

    auto cube = create_cube_surface_mesh();

    rc::RayCaster caster(rc::SceneFlags::Robust, rc::BuildQuality::High);
    caster.add_mesh(std::move(cube));
    caster.commit_updates();

    const int order = 20;
    int hit_count = 0;
    int total = 0;
    const Eigen::Vector3f axis = Eigen::Vector3f(1, 1, 1).normalized();

    for (int k = 0; k <= order; ++k) {
        float angle = float(k) / float(order) * 2.f * float(lagrange::internal::pi);
        Eigen::Affine3f t(Eigen::AngleAxisf(angle, axis));
        caster.update_transform(0, 0, t);
        caster.commit_updates();

        for (int i = 0; i <= order; ++i) {
            float theta = float(i) / float(order) * 2.f * float(lagrange::internal::pi);
            for (int j = 0; j <= order; ++j) {
                float phi = float(j) / float(order) * float(lagrange::internal::pi) -
                            float(lagrange::internal::pi) * 0.5f;
                Eigen::Vector3f dir(
                    std::cos(phi) * std::cos(theta),
                    std::cos(phi) * std::sin(theta),
                    std::sin(phi));

                auto hit = caster.cast(Eigen::Vector3f::Zero(), dir);
                if (hit.has_value()) {
                    hit_count++;
                }
                total++;
            }
        }
    }

    REQUIRE(hit_count == total);
}

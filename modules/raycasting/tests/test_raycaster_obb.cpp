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
#include <lagrange/SurfaceMesh.h>
#include <lagrange/raycasting/RayCaster.h>
#include <lagrange/raycasting/internal/RayCasterOBBAccess.h>
#include <lagrange/utils/function_ref.h>

#include <lagrange/testing/common.h>

#include <Eigen/Core>

#include <cmath>
#include <set>
#include <vector>

namespace {

/// Flat 5x5 vertex grid in z=0 with two triangles per cell (32 facets).
lagrange::SurfaceMesh<double, uint32_t> make_flat_grid()
{
    using Mesh = lagrange::SurfaceMesh<double, uint32_t>;
    Mesh mesh;

    constexpr int N = 5;
    constexpr double step = 2.0 / (N - 1);

    for (int j = 0; j < N; ++j) {
        for (int i = 0; i < N; ++i) {
            mesh.add_vertex({-1.0 + i * step, -1.0 + j * step, 0.0});
        }
    }
    for (int j = 0; j < N - 1; ++j) {
        for (int i = 0; i < N - 1; ++i) {
            uint32_t v0 = static_cast<uint32_t>(j * N + i);
            uint32_t v1 = v0 + 1;
            uint32_t v2 = v0 + N;
            uint32_t v3 = v2 + 1;
            mesh.add_triangle(v0, v1, v2);
            mesh.add_triangle(v1, v3, v2);
        }
    }
    return mesh;
}

lagrange::raycasting::RayCaster make_raycaster_from_flat_grid(
    lagrange::SurfaceMesh<double, uint32_t>& mesh)
{
    lagrange::raycasting::RayCaster rc;
    rc.add_mesh(mesh);
    rc.commit_updates();
    return rc;
}

void overlap_obb_helper(
    const lagrange::raycasting::RayCaster& rc,
    const Eigen::Vector3f& center,
    const Eigen::Matrix3f& axes,
    const Eigen::Vector3f& half_extents,
    lagrange::function_ref<bool(uint32_t, uint32_t, uint32_t)> callback)
{
    lagrange::raycasting::internal::RayCasterOBBAccess::OrientedBox obb;
    obb.center = center;
    obb.axes = axes;
    obb.half_extents = half_extents;
    lagrange::raycasting::internal::RayCasterOBBAccess::overlap_obb(rc, obb, callback);
}

} // namespace

TEST_CASE("OBB overlap: enclosing OBB returns all facets", "[raycasting][obb]")
{
    auto mesh = make_flat_grid();
    auto rc = make_raycaster_from_flat_grid(mesh);

    std::set<uint32_t> found;
    overlap_obb_helper(
        rc,
        Eigen::Vector3f(0, 0, 0),
        Eigen::Matrix3f::Identity(),
        Eigen::Vector3f(2, 2, 2),
        [&](uint32_t, uint32_t, uint32_t facet_index) -> bool {
            found.insert(facet_index);
            return true;
        });

    REQUIRE(found.size() == mesh.get_num_facets());
}

TEST_CASE("OBB overlap: small OBB near center returns subset", "[raycasting][obb]")
{
    auto mesh = make_flat_grid();
    auto rc = make_raycaster_from_flat_grid(mesh);

    std::set<uint32_t> found;
    overlap_obb_helper(
        rc,
        Eigen::Vector3f(0, 0, 0),
        Eigen::Matrix3f::Identity(),
        Eigen::Vector3f(0.3f, 0.3f, 0.1f),
        [&](uint32_t, uint32_t, uint32_t facet_index) -> bool {
            found.insert(facet_index);
            return true;
        });

    REQUIRE(found.size() > 0);
    REQUIRE(found.size() < mesh.get_num_facets());
}

TEST_CASE("OBB overlap: far away OBB returns nothing", "[raycasting][obb]")
{
    auto mesh = make_flat_grid();
    auto rc = make_raycaster_from_flat_grid(mesh);

    std::set<uint32_t> found;
    overlap_obb_helper(
        rc,
        Eigen::Vector3f(100, 100, 100),
        Eigen::Matrix3f::Identity(),
        Eigen::Vector3f(0.5f, 0.5f, 0.5f),
        [&](uint32_t, uint32_t, uint32_t facet_index) -> bool {
            found.insert(facet_index);
            return true;
        });

    REQUIRE(found.empty());
}

TEST_CASE("OBB overlap: rotated OBB finds facets", "[raycasting][obb]")
{
    auto mesh = make_flat_grid();
    auto rc = make_raycaster_from_flat_grid(mesh);

    // 45-degree rotation around Z
    float c = std::cos(3.14159265f / 4.0f);
    float s = std::sin(3.14159265f / 4.0f);
    Eigen::Matrix3f rot;
    rot << c, -s, 0, s, c, 0, 0, 0, 1;

    std::set<uint32_t> found;
    overlap_obb_helper(
        rc,
        Eigen::Vector3f(0, 0, 0),
        rot,
        Eigen::Vector3f(0.5f, 0.5f, 0.1f),
        [&](uint32_t, uint32_t, uint32_t facet_index) -> bool {
            found.insert(facet_index);
            return true;
        });

    REQUIRE(found.size() > 0);
}

TEST_CASE("OBB overlap: early stop returns exactly 1 facet", "[raycasting][obb]")
{
    auto mesh = make_flat_grid();
    auto rc = make_raycaster_from_flat_grid(mesh);

    std::vector<uint32_t> found;
    overlap_obb_helper(
        rc,
        Eigen::Vector3f(0, 0, 0),
        Eigen::Matrix3f::Identity(),
        Eigen::Vector3f(2, 2, 2),
        [&](uint32_t, uint32_t, uint32_t facet_index) -> bool {
            found.push_back(facet_index);
            return false; // stop after first
        });

    REQUIRE(found.size() == 1);
}

// ============================================================================
// 16-lane packet OBB overlap tests
// ============================================================================

namespace {

using OrientedBox = lagrange::raycasting::internal::RayCasterOBBAccess::OrientedBox;

OrientedBox make_aabb(const Eigen::Vector3f& center, const Eigen::Vector3f& half_extents)
{
    OrientedBox obb;
    obb.center = center;
    obb.axes = Eigen::Matrix3f::Identity();
    obb.half_extents = half_extents;
    return obb;
}

} // namespace

TEST_CASE("OBB overlap16: per-lane hit routing matches scalar path", "[raycasting][obb]")
{
    auto mesh = make_flat_grid();
    auto rc = make_raycaster_from_flat_grid(mesh);

    // Three lanes: enclosing, small near origin, far away. Remaining 13 lanes inactive.
    std::vector<OrientedBox> obbs = {
        make_aabb({0, 0, 0}, {2, 2, 2}),
        make_aabb({0, 0, 0}, {0.3f, 0.3f, 0.1f}),
        make_aabb({100, 100, 100}, {0.5f, 0.5f, 0.5f}),
    };

    std::array<std::set<uint32_t>, 16> hits_per_lane;
    lagrange::raycasting::internal::RayCasterOBBAccess::overlap_obb16(
        rc,
        lagrange::span<const OrientedBox>(obbs.data(), obbs.size()),
        obbs.size(),
        [&](uint32_t lane, uint32_t, uint32_t, uint32_t facet_index) -> bool {
            hits_per_lane[lane].insert(facet_index);
            return true;
        });

    REQUIRE(hits_per_lane[0].size() == mesh.get_num_facets());
    REQUIRE(hits_per_lane[1].size() > 0);
    REQUIRE(hits_per_lane[1].size() < mesh.get_num_facets());
    REQUIRE(hits_per_lane[2].empty());
    for (size_t i = 3; i < 16; ++i) {
        REQUIRE(hits_per_lane[i].empty());
    }
}

TEST_CASE("OBB overlap16: explicit mask disables selected lanes", "[raycasting][obb]")
{
    auto mesh = make_flat_grid();
    auto rc = make_raycaster_from_flat_grid(mesh);

    // Three enclosing OBBs, but only lanes 0 and 2 are enabled by the mask.
    std::vector<OrientedBox> obbs = {
        make_aabb({0, 0, 0}, {2, 2, 2}),
        make_aabb({0, 0, 0}, {2, 2, 2}),
        make_aabb({0, 0, 0}, {2, 2, 2}),
    };

    lagrange::raycasting::RayCaster::Mask16 mask = lagrange::raycasting::RayCaster::Mask16::Zero();
    mask[0] = true;
    mask[2] = true;

    std::array<size_t, 16> hits_per_lane{};
    lagrange::raycasting::internal::RayCasterOBBAccess::overlap_obb16(
        rc,
        lagrange::span<const OrientedBox>(obbs.data(), obbs.size()),
        mask,
        [&](uint32_t lane, uint32_t, uint32_t, uint32_t) -> bool {
            ++hits_per_lane[lane];
            return true;
        });

    REQUIRE(hits_per_lane[0] == mesh.get_num_facets());
    REQUIRE(hits_per_lane[1] == 0); // masked off
    REQUIRE(hits_per_lane[2] == mesh.get_num_facets());
}

TEST_CASE("OBB overlap16: early stop on one lane does not affect others", "[raycasting][obb]")
{
    auto mesh = make_flat_grid();
    auto rc = make_raycaster_from_flat_grid(mesh);

    // Two enclosing OBBs; lane 0 stops after the first hit, lane 1 collects everything.
    std::vector<OrientedBox> obbs = {
        make_aabb({0, 0, 0}, {2, 2, 2}),
        make_aabb({0, 0, 0}, {2, 2, 2}),
    };

    std::array<size_t, 16> hits_per_lane{};
    lagrange::raycasting::internal::RayCasterOBBAccess::overlap_obb16(
        rc,
        lagrange::span<const OrientedBox>(obbs.data(), obbs.size()),
        obbs.size(),
        [&](uint32_t lane, uint32_t, uint32_t, uint32_t) -> bool {
            ++hits_per_lane[lane];
            return lane != 0; // stop lane 0 immediately; let lane 1 keep going
        });

    REQUIRE(hits_per_lane[0] == 1);
    REQUIRE(hits_per_lane[1] == mesh.get_num_facets());
}

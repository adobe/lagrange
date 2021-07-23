/*
 * Copyright 2020 Adobe. All rights reserved.
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
#include <lagrange/ui/utils/bounds.h>

namespace ui = lagrange::ui;

TEST_CASE("get_nearest_bounds_distance", "[ui][bounds]")
{
    auto check_nearest_bounds_distance = [](
        const ui::Registry& r,
        const Eigen::Vector3f& from,
        float expected_distance)
    {
        ui::Layer visible_layers(true);
        ui::Layer hidden_layers(false);
        float nearest = ui::get_nearest_bounds_distance(r, from, visible_layers, hidden_layers);
        REQUIRE(nearest == Approx(expected_distance));
    };

    SECTION("no bounds")
    {
        ui::Registry registry_without_bounds;
        const Eigen::Vector3f from(1, 1, 1);
        check_nearest_bounds_distance(registry_without_bounds, from, -1);
    }

    ui::Registry registry_with_bounds;
    ui::Entity e = registry_with_bounds.create();
    Eigen::Vector3f min(0, 0, 0);
    Eigen::Vector3f max(10, 10, 10);
    ui::AABB bb(Eigen::AlignedBox3f(min, max));
    registry_with_bounds.emplace<ui::Bounds>(e, ui::Bounds{bb, bb, bb});

    SECTION("on min corner")
    {
        const Eigen::Vector3f from(0, 0, 0);
        check_nearest_bounds_distance(registry_with_bounds, from, 0);
    }

    SECTION("on max corner")
    {
        const Eigen::Vector3f from(10, 10, 10);
        check_nearest_bounds_distance(registry_with_bounds, from, 0);
    }

    SECTION("inside bounds near min corner")
    {
        const Eigen::Vector3f from(1, 1, 1);
        check_nearest_bounds_distance(registry_with_bounds, from, 0);
    }

    SECTION("inside bounds near max corner")
    {
        const Eigen::Vector3f from(9, 9, 9);
        check_nearest_bounds_distance(registry_with_bounds, from, 0);
    }

    SECTION("outside bounds near min corner")
    {
        const Eigen::Vector3f from(-1, -1, -1);
        check_nearest_bounds_distance(registry_with_bounds, from, std::sqrt(3));
    }

    SECTION("outside bounds near max corner")
    {
        const Eigen::Vector3f from(11, 11, 11);
        check_nearest_bounds_distance(registry_with_bounds, from, std::sqrt(3));
    }
}

TEST_CASE("get_furthest_bounds_distance", "[ui][bounds]")
{
    auto check_furthest_bounds_distance = [](
        const ui::Registry& r,
        const Eigen::Vector3f& from,
        float expected_distance)
    {
        ui::Layer visible_layers(true);
        ui::Layer hidden_layers(false);
        float furthest = ui::get_furthest_bounds_distance(r, from, visible_layers, hidden_layers);
        REQUIRE(furthest == Approx(expected_distance));
    };

    SECTION("no bounds")
    {
        ui::Registry registry_without_bounds;
        check_furthest_bounds_distance(registry_without_bounds, Eigen::Vector3f(1, 1, 1), -1);
    }

    ui::Registry registry_with_bounds;
    ui::Entity e = registry_with_bounds.create();
    Eigen::Vector3f min(0, 0, 0);
    Eigen::Vector3f max(10, 10, 10);
    ui::AABB bb(Eigen::AlignedBox3f(min, max));
    registry_with_bounds.emplace<ui::Bounds>(e, ui::Bounds{bb, bb, bb});

    SECTION("on min corner")
    {
        const Eigen::Vector3f from(0, 0, 0);
        check_furthest_bounds_distance(registry_with_bounds, from, std::sqrt(300));
    }

    SECTION("on max corner")
    {
        const Eigen::Vector3f from(10, 10, 10);
        check_furthest_bounds_distance(registry_with_bounds, from, std::sqrt(300));
    }

    SECTION("inside bounds near min corner")
    {
        const Eigen::Vector3f from(1, 1, 1);
        check_furthest_bounds_distance(registry_with_bounds, from, std::sqrt(243));
    }

    SECTION("inside bounds near max corner")
    {
        const Eigen::Vector3f from(9, 9, 9);
        check_furthest_bounds_distance(registry_with_bounds, from, std::sqrt(243));
    }

    SECTION("outside bounds near min corner")
    {
        const Eigen::Vector3f from(-1, -1, -1);
        check_furthest_bounds_distance(registry_with_bounds, from, std::sqrt(363));
    }

    SECTION("outside bounds near max corner")
    {
        const Eigen::Vector3f from(11, 11, 11);
        check_furthest_bounds_distance(registry_with_bounds, from, std::sqrt(363));
    }
}

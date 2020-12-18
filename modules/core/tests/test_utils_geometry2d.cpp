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

#include <lagrange/utils/geometry2d.h>
#include <Eigen/Core>

TEST_CASE("utils-geometry2d")
{
    using namespace lagrange;
    using Vec2 = Eigen::Vector2d;


    REQUIRE(sqr_minimum_distance(Vec2(0, 0), Vec2(0, 1), Vec2(0, 0)) == 0.0);
    REQUIRE(sqr_minimum_distance(Vec2(0, 0), Vec2(0, 1), Vec2(0, 1)) == 0.0);
    REQUIRE(sqr_minimum_distance(Vec2(0, 0), Vec2(0, 1), Vec2(0, .5)) == 0.0);

    REQUIRE(sqr_minimum_distance(Vec2(0, 0), Vec2(0, 1), Vec2(1, 0)) == (1 * 1));
    REQUIRE(sqr_minimum_distance(Vec2(0, 0), Vec2(0, 1), Vec2(1, .2)) == (1 * 1));

    REQUIRE(sqr_minimum_distance(Vec2(0, 0), Vec2(0, 1), Vec2(0, -1)) == (1 * 1));
    REQUIRE(sqr_minimum_distance(Vec2(0, 0), Vec2(0, 1), Vec2(0, 2)) == (1 * 1));
    REQUIRE(sqr_minimum_distance(Vec2(0, 0), Vec2(0, 1), Vec2(0, 3)) == (2 * 2));

    REQUIRE(sqr_minimum_distance(Vec2(0, 0), Vec2(1, 1), Vec2(0, 1)) == .5);
}

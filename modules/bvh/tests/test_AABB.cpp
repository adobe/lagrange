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
#include <lagrange/bvh/AABB.h>
#include <lagrange/testing/common.h>

TEST_CASE("AABB", "[bvh][aabb]")
{
    using namespace lagrange;

    std::vector<lagrange::bvh::AABB<float, 3>::Index> results;

    auto Point3 = [](float x, float y, float z) { return Eigen::Vector3f(x, y, z); };

    SECTION("empty")
    {
        lagrange::bvh::AABB<float, 3>::Box unit_box(Point3(0, 0, 0), Point3(1, 1, 1));

        lagrange::bvh::AABB<float, 3> aabb;
        aabb.build({});
        aabb.intersect(unit_box, results);
        REQUIRE(results.empty());
    }

    SECTION("single box")
    {
        lagrange::bvh::AABB<float, 3>::Box unit_box(Point3(0, 0, 0), Point3(1, 1, 1));
        lagrange::bvh::AABB<float, 3>::Box query_box(
            Point3(0.5f, 0.5f, 0.5f),
            Point3(1.5f, 1.5f, 1.5f));
        std::vector<lagrange::bvh::AABB<float, 3>::Box> boxes = {unit_box};

        lagrange::bvh::AABB<float, 3> aabb;
        aabb.build(boxes);
        aabb.intersect(query_box, results);
        REQUIRE(!results.empty());
        REQUIRE(results[0] == 0);
    }

    SECTION("three boxes")
    {
        lagrange::bvh::AABB<float, 3>::Box box1(Point3(0, 0, 0), Point3(1, 1, 1));
        lagrange::bvh::AABB<float, 3>::Box box2(Point3(1, 1, 1), Point3(2, 2, 2));
        lagrange::bvh::AABB<float, 3>::Box box3(Point3(3, 3, 3), Point3(4, 4, 4));
        std::vector<lagrange::bvh::AABB<float, 3>::Box> boxes = {box1, box2, box3};

        lagrange::bvh::AABB<float, 3>::Box query_box(
            Point3(0.5f, 0.5f, 0.5f),
            Point3(1.5f, 1.5f, 1.5f));

        lagrange::bvh::AABB<float, 3> aabb;
        aabb.build(boxes);
        aabb.intersect(query_box, results);
        REQUIRE(results.size() == 2);
    }
}

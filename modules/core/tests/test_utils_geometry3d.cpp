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

#include <lagrange/utils/geometry3d.h>

using namespace Eigen;
using namespace lagrange;

TEST_CASE("utils-geometry3d")
{
    Vector3d v1(0, 0, 1);
    REQUIRE(cos_angle_between(v1, Vector3d(0, 0, 1)) == 1.0);
    REQUIRE(cos_angle_between(v1, Vector3d(1, 0, 0)) == 0.0);
    REQUIRE(cos_angle_between(v1, Vector3d(0, 0, -1)) == -1.0);

    Vector3f v2(0, 0, 1);
    REQUIRE(cos_angle_between(v2, Vector3f(0, 0, 1)) == 1.0);
    REQUIRE(cos_angle_between(v2, Vector3f(1, 0, 0)) == 0.0);
    REQUIRE(cos_angle_between(v2, Vector3f(0, 0, -1)) == -1.0);

    REQUIRE(angle_between(Vector3d(0, 0, 1), Vector3d(0, 0, 1)) == 0);
    REQUIRE(angle_between(Vector3d(0, 0, 1), Vector3d(1, 0, 0)) == M_PI_2);
    REQUIRE(angle_between(Vector3d(0, 0, 1), Vector3d(0, 0, -1)) == M_PI);

    REQUIRE(project_on_line(Vector3d(1, 1, 1), Vector3d(1, 0, 0)).isApprox(Vector3d(1, 0, 0)));

    REQUIRE(project_on_plane(Vector3d(1, 1, 1), Vector3d(0, 1, 0)).isApprox(Vector3d(1, 0, 1)));
    REQUIRE(project_on_plane(Vector3d(2, 2, 2), Vector3d(0, 1, 0)).isApprox(Vector3d(2, 0, 2)));

    REQUIRE(
        projected_cos_angle_between(Vector3d(1, 1, 1), Vector3d(1, 1, -1), Vector3d(0, 1, 0)) ==
        0.0);
}

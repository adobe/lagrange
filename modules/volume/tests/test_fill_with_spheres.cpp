/*
 * Copyright 2021 Adobe. All rights reserved.
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
#include <lagrange/testing/common.h>
#include <lagrange/volume/fill_with_spheres.h>
#include <lagrange/volume/mesh_to_volume.h>

TEST_CASE("fill_with_spheres: reproducibility", "[volume]")
{
    auto mesh = lagrange::create_cube();
    Eigen::MatrixXf spheres1;
    Eigen::MatrixXf spheres2;
    auto grid = lagrange::volume::mesh_to_volume(*mesh, 0.1);
    int max_spheres = 100;
    lagrange::volume::fill_with_spheres(*grid, spheres1, max_spheres);
    lagrange::volume::fill_with_spheres(*grid, spheres2, max_spheres);
    REQUIRE(spheres1.rows() >= 1);
    REQUIRE(spheres1.rows() <= max_spheres);
    REQUIRE(spheres1 == spheres2);
}

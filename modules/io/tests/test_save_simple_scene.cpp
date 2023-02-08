/*
 * Copyright 2023 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include "io_common.h"

#include <lagrange/testing/common.h>
#include <lagrange/io/save_simple_scene.h>

using namespace lagrange;

scene::SimpleScene32d3 create_simple_scene() {
    auto cube = io::testing::create_surfacemesh_cube();
    auto sphere = io::testing::create_surfacemesh_sphere();

    scene::SimpleScene32d3 scene;
    auto cube_idx = scene.add_mesh(std::move(cube));
    auto sphere_idx = scene.add_mesh(std::move(sphere));

    using AffineTransform = typename decltype(scene)::AffineTransform;
    AffineTransform t1 = AffineTransform::Identity();
    AffineTransform t2 = AffineTransform::Identity();
    AffineTransform t3 = AffineTransform::Identity();
    t1.translate(Eigen::Vector3d(0, -3, 0));
    t2.translate(Eigen::Vector3d(3, 0, 0));
    t3.translate(Eigen::Vector3d(-3, 0, 0));
    scene.add_instance({cube_idx, t1});
    scene.add_instance({cube_idx, t2});
    scene.add_instance({sphere_idx, t3});

    return scene;
}

TEST_CASE("save_simple_scene", "[io]")
{
    auto scene = create_simple_scene();
    io::SaveOptions opt;
    opt.encoding = io::FileEncoding::Ascii;
    REQUIRE_NOTHROW(io::save_simple_scene("scene.gltf", scene, opt));
}

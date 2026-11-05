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
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/load_scene.h>
#include <lagrange/io/load_simple_scene.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/io/save_scene.h>
#include <lagrange/io/save_simple_scene.h>
#include <lagrange/scene/SceneTypes.h>
#include <lagrange/scene/SimpleSceneTypes.h>
#include <lagrange/scene/scene_convert.h>
#include <lagrange/scene/simple_scene_convert.h>
#include <lagrange/testing/check_scenes_equal.h>
#include <lagrange/testing/common.h>

#include <catch2/generators/catch_generators.hpp>

#include <stdexcept>
#include <string>

using namespace lagrange;

TEST_CASE("serialized IO extensions", "[io][serialization2]")
{
    const std::string extension = GENERATE(".lgm", ".lgs", ".lgx");
    CAPTURE(extension);
    const auto path = testing::get_test_output_path("test_serialized_io/triangle" + extension);

    SurfaceMesh32f mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);
    auto scene = scene::mesh_to_scene(mesh);
    scene.name = "serialized_scene";
    auto simple_scene = scene::mesh_to_simple_scene(mesh);

    if (extension == ".lgm") {
        io::save_mesh(path, mesh);
    } else if (extension == ".lgs") {
        REQUIRE_THROWS_AS(io::save_scene(path, scene), std::runtime_error);
        io::save_simple_scene(path, simple_scene);
    } else {
        io::save_scene(path, scene);
    }

    const auto loaded_mesh = io::load_mesh<SurfaceMesh32f>(path);
    const auto expected_mesh = extension == ".lgx" ? scene::scene_to_mesh(scene) : mesh;
    testing::check_meshes_equal(expected_mesh, loaded_mesh);

    const auto loaded_simple_scene = io::load_simple_scene<scene::SimpleScene32f3>(path);
    REQUIRE(loaded_simple_scene.get_num_meshes() == 1);
    REQUIRE(loaded_simple_scene.compute_num_instances() == 1);
    testing::check_meshes_equal(mesh, loaded_simple_scene.get_mesh(0));

    const auto loaded_scene = io::load_scene<scene::Scene32f>(path);
    if (extension == ".lgx") {
        testing::check_scenes_equal(scene, loaded_scene);
    } else {
        REQUIRE(loaded_scene.meshes.size() == 1);
        testing::check_meshes_equal(mesh, loaded_scene.meshes[0]);
    }
}

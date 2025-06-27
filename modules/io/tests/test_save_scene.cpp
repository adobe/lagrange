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
#include <lagrange/io/load_scene.h>
#include <lagrange/io/save_scene.h>
#include <lagrange/testing/common.h>

#include <sstream>

using namespace lagrange;

TEST_CASE("save_scene", "[io]")
{
    using SceneType = scene::Scene32f;

    SECTION("Avocado")
    {
        fs::path avocado_path = testing::get_data_path("open/io/avocado/Avocado.gltf");
        auto scene = io::load_scene<SceneType>(avocado_path);

        std::stringstream ss;
        io::save_scene(ss, scene, io::FileFormat::Gltf);
        auto scene2 = io::load_scene<SceneType>(ss);

        REQUIRE(scene.name == scene2.name);
        REQUIRE(scene.nodes.size() == scene2.nodes.size());
        REQUIRE(scene.root_nodes.size() == scene2.root_nodes.size());
        REQUIRE(scene.images.size() == scene2.images.size());
        REQUIRE(scene.textures.size() == scene2.textures.size());
        REQUIRE(scene.materials.size() == scene2.materials.size());
        REQUIRE(scene.lights.size() == scene2.lights.size());
        REQUIRE(scene.cameras.size() == scene2.cameras.size());
        REQUIRE(scene.skeletons.size() == scene2.skeletons.size());
        REQUIRE(scene.animations.size() == scene2.animations.size());
    }
}


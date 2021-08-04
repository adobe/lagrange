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

#include <lagrange/ui/UI.h>
#include <CLI/CLI.hpp>

namespace ui = lagrange::ui;

int main(int argc, char** argv)
{
    ui::Viewer v("UI Example - Tree Node", 1920, 1080);

    // Creates an empty scene node
    auto a = ui::create_scene_node(v, "Empty scene node");

    // Creates an empty scene node with a as parent
    ui::create_scene_node(v, "Another node", a);

    // Light is another scene node, with <LightComponent>
    auto light = ui::add_directional_light(v);
    ui::set_name(v, light, "Light Node");

    // Group under a scene node
    [[maybe_unused]] auto group = ui::group(v, {a, light}, "Group");

    // Ungroup (and remove new parent)
    // ui::ungroup(v, group, true);


    {
        auto top_level = ui::create_scene_node(v, "Top level");

        // Create nodes recursively
        ui::Entity parent = top_level;
        for (auto i = 0; i < 5; i++) {
            parent = ui::create_scene_node(v, "Recursive", parent);
        }

        // Remove recursively
        ui::remove(v, top_level, true);
    }


    v.run();

    return 0;
}

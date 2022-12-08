/*
 * Copyright 2022 Adobe. All rights reserved.
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
#include <fstream>

namespace ui = lagrange::ui;

int main(int argc, char** argv)
{
    ui::Viewer viewer(argc, argv);


    auto& keybinds = ui::get_keybinds(viewer);

    // Register single key action that can be later queried
    keybinds.add("my_global_action", {ImGuiKey_F});

    // Register multi-key action that can be later queried
    keybinds.add("my_global_action_with_modifier_key", {ImGuiKey_F, {ImGuiKey_LeftCtrl}});

    // Register action specific to the viewport (active only when viewport is hovered)
    keybinds.add("viewport.my_viewport_action", {ImGuiKey_F});

    // Keybinds can be serialized/deserialized:
    {
        std::ofstream f("keybinds.json");
        keybinds.save(f);
    }
    {
        std::ifstream f("keybinds.json");
        keybinds.load(f);
    }
    

    viewer.run([](ui::Registry& r) {
        const auto& keybinds = ui::get_keybinds(r);

        // Query actions
        {
            if (keybinds.is_released("my_global_action")) {
                lagrange::logger().info("my_global_action key was released");
            }

            if (keybinds.is_pressed("my_global_action_with_modifier_key")) {
                lagrange::logger().info("my_global_action_with_modifier_key key was pressed");
            }

            if (keybinds.is_down("viewport.my_viewport_action")) {
                lagrange::logger().info("my_viewport_action key is down");
            }
        }

        // Query keys directly
        if (keybinds.is_pressed(ImGuiKey_UpArrow)) {
            lagrange::logger().info("ImGuiKey_UpArrow was pressed");
        }

        return true;
    });
    return 0;
}

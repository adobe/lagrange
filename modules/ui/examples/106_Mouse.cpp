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

namespace ui = lagrange::ui;

int main(int argc, char** argv)
{
    ui::Viewer viewer(argc, argv);

    viewer.run([&](ui::Registry& r) {
        // Get current mouse state
        const auto& mouse = ui::get_mouse(r);

        lagrange::logger().info("Mouse position: {} {}", mouse.position.x(), mouse.position.y());
        lagrange::logger().info("Mouse position delta: {} {}", mouse.delta.x(), mouse.delta.y());
        lagrange::logger().info("Mouse wheel x: {} y: {}", mouse.wheel, mouse.wheel_horizontal);

        // For querying mouse buttons, use keybinds:
        const auto& keybinds = ui::get_keybinds(r);
        if (keybinds.is_pressed(GLFW_MOUSE_BUTTON_LEFT)) {
            lagrange::logger().info("Left mouse button pressed");
        }

        return true;
    });
    return 0;
}

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

    // Option A)
    // Runs the main loop
    // viewer.run();

    // Option B)
    // Runs the main loop with this function at the beginning of every frame
    viewer.run([&](ui::Registry& r) {
        const ui::GlobalTime& viewer_time = ui::get_time(r);

        lagrange::logger().info(
            "Time from start {}s, from last frame: {}s",
            viewer_time.t,
            viewer_time.dt);

        // Return true if the main loop should keep going
        return true;
    });
    return 0;
}

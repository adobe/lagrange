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

#include <lagrange/ui/UI.h>
#include <CLI/CLI.hpp>


namespace ui = lagrange::ui;

int main(int argc, char** argv)
{
    struct
    {
        std::string input;
        bool verbose = false;
    } args;

    CLI::App app{argv[0]};
    app.add_option("input", args.input, "Input scene");
    app.add_flag("-v", args.verbose, "Verbose");
    CLI11_PARSE(app, argc, argv)

    if (args.verbose) {
        lagrange::logger().set_level(spdlog::level::debug);
    }

    ui::Viewer viewer("UI Example - Scene", 1920, 1080);

#ifdef LAGRANGE_WITH_ASSIMP
    if (!args.input.empty()) {
        ui::load_scene<lagrange::TriangleMesh3Df>(viewer, args.input);
        ui::camera_focus_and_fit(viewer, ui::get_focused_camera_entity(viewer));
    }
#else
    lagrange::logger().error("Load scene is only available with Assimp.");
#endif

    viewer.run();

    return 0;
}

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
    ui::Viewer viewer(argc, argv);

    // Load to memory
    auto mesh_entity = ui::load_obj<lagrange::TriangleMesh3D>(
        viewer,
        LAGRANGE_DATA_FOLDER "/open/core/rounded_cube.obj");

    // Show in scene
    ui::show_mesh(viewer, mesh_entity);

    // Center camera on the mesh
    ui::camera_focus_and_fit(viewer);

    viewer.run();
    return 0;
}

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
namespace ui = lagrange::ui;

int main(int argc, char** argv)
{
    ui::Viewer viewer(argc, argv);

    static bool show_imgui_demo = false;

    ui::add_panel(viewer, "Custom UI Panel", [&]() {
        ImGui::Text("Place ImGui widgets here");

        if (ImGui::Button("Open ImGui Demo Window")) {
            show_imgui_demo = true;
        }

        if (show_imgui_demo) {
            ImGui::ShowDemoWindow(&show_imgui_demo);
        }
    });

    viewer.run();
    return 0;
}

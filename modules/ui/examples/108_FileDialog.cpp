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

    ui::add_panel(viewer, "File Dialogs", []() {
        if (ImGui::Button("Load single file")) {
            const auto path = ui::open_file("Load single file");
            lagrange::logger().info("Path: {}", path.string());
        }

        if (ImGui::Button("Load image file")) {
            const auto path = ui::open_file(
                "Load image file",
                ".",
                {{"Image Files", "*.png *.jpg *.jpeg *.bmp"}});
            lagrange::logger().info("Image Path: {}", path.string());
        }

        if (ImGui::Button("Load multiple files")) {
            const auto paths = ui::open_files("Load multiple files");
            lagrange::logger().info("Paths:");
            for (const auto & path : paths) {
                lagrange::logger().info("{}", path.string());
            }
        }

        if (ImGui::Button("Save single file")) {
            const auto path = ui::save_file("Save single file", ".", {{"Text Files", "*.txt"}});
            lagrange::logger().info("Path: {}", path.string());

            if (!path.empty()) {
                {
                    lagrange::fs::ofstream f(path.path());
                    f << "Lorem ipsum dolor sit amet";
                }
            }
        }
    });

    viewer.run();
    return 0;
}

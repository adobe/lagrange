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
#include <lagrange/fs/file_utils.h>
#include <lagrange/ui/default_keybinds.h>
#include <lagrange/ui/imgui/UIWidget.h>
#include <lagrange/ui/imgui/buttons.h>
#include <lagrange/ui/panels/KeybindsPanel.h>
#include <lagrange/ui/utils/input.h>
#include <lagrange/ui/utils/uipanel.h>
#include <lagrange/utils/safe_cast.h>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <nfd.h>
#include <fstream>

namespace lagrange {
namespace ui {
namespace {


void keybinds_panel_system(Registry& registry, Entity /*e*/)
{
    auto& k = get_keybinds(registry);
    auto& mapping = k.get();

    float w = ImGui::GetContentRegionAvail().x - 20.0f;
    {
        ImGui::Text("Keybinds");

        if (ImGui::Button("Load", ImVec2(w / 3, 0))) {
            nfdchar_t* path = nullptr;
            nfdresult_t result = NFD_OpenDialog("json", nullptr, &path);
            if (result == NFD_OKAY) {
                auto path_string = std::string(path);
                std::ifstream f(path_string);
                if (f.good()) {
                    k.load(f);
                }
                free(path);
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Save", ImVec2(w / 3, 0))) {
            nfdchar_t* path = nullptr;
            nfdresult_t result = NFD_SaveDialog("json", nullptr, &path);
            if (result == NFD_OKAY) {
                std::string s = fs::get_string_ending_with(path, ".json");

                std::ofstream f(s);
                if (f.good()) {
                    k.save(f);
                }

                free(path);
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Reset", ImVec2(w / 3, 0))) {
            k = initialize_default_keybinds();
        }

        ImGui::Separator();
    }

    {
        ImGui::Text("Camera Preset");
        if (button_icon(
                has_camera_scheme(k, DefaultCameraScheme::DIMENSION),
                "Dimension",
                "",
                "",
                &k,
                true,
                {w / 4, 0})) {
            set_camera_scheme(k, DefaultCameraScheme::DIMENSION);
        }
        ImGui::SameLine();
        if (button_icon(
                has_camera_scheme(k, DefaultCameraScheme::MAYA),
                "Maya",
                "",
                "",
                &k,
                true,
                {w / 4, 0})) {
            set_camera_scheme(k, DefaultCameraScheme::MAYA);
        }
        ImGui::SameLine();
        if (button_icon(
                has_camera_scheme(k, DefaultCameraScheme::BLENDER),
                "Blender",
                "",
                "",
                &k,
                true,
                {w / 4, 0})) {
            set_camera_scheme(k, DefaultCameraScheme::BLENDER);
        }
        ImGui::SameLine();
        if (button_icon(
                has_camera_scheme(k, DefaultCameraScheme::SUBSTANCE),
                "Substance",
                "",
                "",
                &k,
                true,
                {w / 4, 0})) {
            set_camera_scheme(k, DefaultCameraScheme::SUBSTANCE);
        }

        ImGui::Separator();
    }

    ImGui::Text("Actions");

    {
        std::vector<std::string> items;
        for (auto it : mapping) items.push_back(it.first);

        static int selected = 0;
        bool remove_selected_action = false;


        static std::string new_action_name = "";
        if (ImGui::Button("Add Action")) {
            k.register_action(new_action_name);
            new_action_name = "";
        }

        ImGui::SameLine();
        ImGui::InputText("Action Name", &new_action_name);


        if (ImGui::Button("Remove Action")) {
            remove_selected_action = true;
        }

        ImGui::ListBox(
            "Actions",
            &selected,
            [](void* data, int idx, const char** out_text) {
                auto& _items = *reinterpret_cast<std::vector<std::string>*>(data);
                if (out_text) *out_text = _items[idx].c_str();
                return true;
            },
            &items,
            int(mapping.size()));


        static bool adding = false;

        // Disable while editing
        k.enable(!adding);

        if (selected >= 0 && selected < int(items.size())) {
            const std::string& sel_action = items[selected];
            auto& action_binds = mapping.at(sel_action);

            ImGui::Separator();

            ImGui::Dummy(ImVec2(5, 5));

            for (auto& keybind : action_binds) {
                std::string keybind_str = Keybinds::to_string(keybind);
                ImGui::InputText("Key bind", &keybind_str, ImGuiInputTextFlags_ReadOnly);
            }

            if (action_binds.size() > 0 && ImGui::Button("Remove all")) {
                k.remove(sel_action);
            }

            ImGui::SameLine();
            if (!adding) {
                if (ImGui::Button("Add")) {
                    adding = true;
                }
            } else {
                if (ImGui::Button("Cancel (Esc)")) {
                    adding = false;
                }
            }

            if (adding) {
                ImGui::Separator();

                Keybinds::Keybind tmp_keybind(-1, {});

                for (auto i = 0; i < Keybinds::keymap_size; i++) {
                    if (safe_cast<size_t>(tmp_keybind.modifier_count) ==
                        tmp_keybind.modifiers.max_size())
                        break;

                    if (i == GLFW_KEY_ESCAPE || i == GLFW_KEY_ENTER) continue;

                    if (ImGui::IsKeyDown(i)) {
                        tmp_keybind.modifiers[tmp_keybind.modifier_count++] = i;
                    }
                }

                // Sort and set last in order key as the main button
                if (tmp_keybind.modifier_count > 0) {
                    std::sort(tmp_keybind.modifiers.rbegin(), tmp_keybind.modifiers.rend());
                    tmp_keybind.button = tmp_keybind.modifiers[--tmp_keybind.modifier_count];
                }

                std::string keybind_str = Keybinds::to_string(tmp_keybind);
                ImGui::InputText("Key bind", &keybind_str, ImGuiInputTextFlags_ReadOnly);

                ImGui::TextColored(
                    ImVec4(ImColor(ImGui::Spectrum::GREEN500).Value),
                    "Press Enter to Save");

                if (ImGui::IsKeyReleased(GLFW_KEY_ESCAPE)) {
                    adding = false;
                }

                if (ImGui::IsKeyReleased(GLFW_KEY_ENTER)) {
                    adding = false;
                    k.add(sel_action, tmp_keybind);
                }
            }

            if (remove_selected_action) {
                k.unregister_action(sel_action);
            }
        }
    }
}


} // namespace

Entity add_keybinds_panel(Registry& r, const std::string& name /*= "Keybinds"*/)
{
    return add_panel(r, name, keybinds_panel_system);
}


} // namespace ui
} // namespace lagrange

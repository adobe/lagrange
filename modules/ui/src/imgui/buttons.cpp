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

#include <lagrange/ui/imgui/buttons.h>

namespace lagrange {
namespace ui {

bool button_unstyled(const char* label)
{
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_BorderShadow, ImVec4(0, 0, 0, 0));
    const bool result = ImGui::SmallButton(label);

    ImGui::PopStyleColor(5);
    return result;
}


bool button_toolbar(
    bool selected,
    const std::string& label,
    const std::string& tooltip,
    const std::string& keybind_id,
    const Keybinds* keybinds,
    bool enabled)
{
    const float size = ImGui::GetContentRegionAvail().x;
    return button_icon(selected, label, tooltip, keybind_id, keybinds, enabled, ImVec2(size, size));
}

bool button_icon(
    bool selected,
    const std::string& label,
    const std::string& tooltip,
    const std::string& keybind_id,
    const Keybinds* keybinds,
    bool enabled,
    ImVec2 size)
{
    bool clicked = false;

    if (keybinds && !keybind_id.empty()) {
        clicked |= keybinds->is_pressed(keybind_id);
    }

    ImGui::PushStyleColor(
        ImGuiCol_Button,
        ImGui::GetStyleColorVec4(selected ? ImGuiCol_Header : ImGuiCol_Button));

    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImColor(ImGui::Spectrum::GRAY100).Value);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_Header));
    }

    if (!enabled) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImColor(ImGui::Spectrum::GRAY500).Value);
        // hovered text: GRAY900
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_Button));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_Button));
    }

    if (ImGui::Button(
            label.c_str(),
            ImVec2(static_cast<float>(size.x), static_cast<float>(size.y)))) {
        clicked = true;
    }

    ImGui::PopStyleColor(1);

    if (!enabled) {
        ImGui::PopStyleColor(3);
    }

    if (selected) {
        ImGui::PopStyleColor(2);
    }

    if (ImGui::IsItemHovered() && !tooltip.empty()) {
        std::string tmp = tooltip;
        if (keybinds && !keybind_id.empty()) {
            std::string keybind_str = keybinds->to_string(keybind_id);
            if (!keybind_str.empty()) {
                tmp += " (" + keybind_str + ")";
            }
        }
        ImGui::SetTooltip("%s", tmp.c_str());
    }

    return clicked;
}

} // namespace ui
} // namespace lagrange

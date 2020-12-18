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
#include <lagrange/ui/ToolbarUI.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <lagrange/ui/UIWidget.h>
#include <lagrange/ui/Viewer.h>
#include <lagrange/utils/strings.h>
#include <lagrange/ui/Viewer.h>

namespace lagrange {
namespace ui {

void ToolbarUI::add_action(const std::string& group_name, const std::shared_ptr<Action> & action)
{
    assert(action);
    m_group_actions[group_name].push_back(action);
}

void ToolbarUI::draw()
{
    const float menubar_height = get_viewer()->get_menubar_height();
    ImGui::SetNextWindowPos(ImVec2(0, menubar_height));
    ImGui::SetNextWindowSize(ImVec2(toolbar_width, ImGui::GetMainViewport()->Size.y - menubar_height));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    this->begin(ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);

    ImGui::PopStyleVar(1);

    get_viewer()->draw_toolbar();

    ImGui::Separator();

    for (auto& group : m_group_actions) {
        for (auto& action_ptr : group.second) {
            auto& action = *action_ptr;
            
            //Draw icon and detect whether it was clicked
            if (button_toolbar(action.selected(),
                action.label,
                action.tooltip,
                action.keybind_action,
                action.enabled)) {

                action.on_click(action);
            }

            if (action.popup) {
                if (ImGui::BeginPopupContextItem(action.label.c_str())) {
                    action.popup(action);
                    ImGui::EndPopup();
                }
            }
        }
        
        ImGui::Separator();
    }

    for (auto& panel : get_viewer()->get_ui_panels()) {
        if (panel->draw_toolbar()) {
            ImGui::Separator();
        }
    }

    this->end();
}

} // namespace ui
} // namespace lagrange

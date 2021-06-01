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

#include <lagrange/ui/components/SelectionContext.h>
#include <lagrange/ui/imgui/UIWidget.h>
#include <lagrange/ui/imgui/buttons.h>
#include <lagrange/ui/panels/ToolbarPanel.h>
#include <lagrange/ui/types/Tools.h>
#include <lagrange/ui/utils/input.h>
#include <lagrange/ui/utils/selection.h>
#include <lagrange/ui/utils/uipanel.h>

#include <IconsFontAwesome5.h>
#include <imgui.h>

namespace lagrange {
namespace ui {
namespace {


void toolbar_panel_system(Registry& registry, Entity e)
{
    auto* keybinds = &get_keybinds(registry);
    auto& tools = registry.ctx<Tools>();


    ImGui::PushID(42);

    {
        auto current_tool_type = tools.current_tool_type();
        for (auto tool_type : tools.get_tool_types()) {
            using namespace entt::literals;
            auto type = entt::resolve(tool_type);

            if (button_toolbar(
                    current_tool_type == type.id(),
                    type.prop("icon"_hs).value().cast<std::string>(),
                    type.prop("display_name"_hs).value().cast<std::string>(),
                    type.prop("keybind"_hs).value().cast<std::string>(),
                    keybinds,
                    true)) {
                tools.set_current_tool_type(type.id());
            }
        }
    }

    ImGui::Separator();

    {
        auto current_element_type = tools.current_element_type();
        for (auto elem_type : tools.get_element_types()) {
            using namespace entt::literals;
            auto type = entt::resolve(elem_type);

            if (button_toolbar(
                    current_element_type == type.id(),
                    type.prop("icon"_hs).value().cast<std::string>(),
                    type.prop("display_name"_hs).value().cast<std::string>(),
                    type.prop("keybind"_hs).value().cast<std::string>(),
                    keybinds,
                    true)) {
                tools.set_current_element_type(type.id());
            }
        }

        auto& sel_ctx = get_selection_context(registry);

        if (button_toolbar(
                sel_ctx.select_backfacing,
                ICON_FA_STEP_BACKWARD,
                "Select Backfaces",
                "global.selection_mode.select_backfaces",
                keybinds,
                true)) {
            sel_ctx.select_backfacing = !sel_ctx.select_backfacing;
        }
    }


    ImGui::PopID();
}

void toolbar_panel_pre(Registry& registry, Entity e)
{
    const float menubar_height = get_menu_height(registry).height;

    ImGui::SetNextWindowPos(ImVec2(0, menubar_height));
    ImGui::SetNextWindowSize(
        ImVec2(ToolbarPanel::toolbar_width, ImGui::GetMainViewport()->Size.y - menubar_height));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
}
void toolbar_panel_post(Registry& registry, Entity e)
{
    ImGui::PopStyleVar();
}


} // namespace


Entity add_toolbar_panel(Registry& r, const std::string& name /*= "Toolbar"*/)
{
    auto e = add_panel(r, name, toolbar_panel_system, toolbar_panel_pre, toolbar_panel_post);
    r.emplace<ToolbarPanel>(e);
    return e;
}

} // namespace ui
} // namespace lagrange

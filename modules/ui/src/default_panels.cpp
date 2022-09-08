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
#include <lagrange/ui/default_panels.h>
#include <lagrange/ui/types/Systems.h>

#include <lagrange/ui/panels/ComponentPanel.h>
#include <lagrange/ui/panels/KeybindsPanel.h>
#include <lagrange/ui/panels/LoggerPanel.h>
#include <lagrange/ui/panels/RendererPanel.h>
#include <lagrange/ui/panels/ScenePanel.h>
#include <lagrange/ui/panels/ToolbarPanel.h>
#include <lagrange/ui/panels/ViewportPanel.h>


#include <lagrange/ui/default_entities.h>

#include <imgui.h>

// Docking API is currently internal only
// Change to public API once imgui docking is finished and in master
#include <imgui_internal.h>


namespace lagrange {
namespace ui {

DefaultPanels add_default_panels(Registry& registry)
{
    DefaultPanels res;
    res.logger = add_logger_panel(registry);
    res.scene = add_scene_panel(registry, "Scene");
    res.viewport = add_viewport_panel(registry, "Viewport", NullEntity);
    res.renderer = add_renderer_panel(registry, "Renderer");
    res.components = add_component_panel(registry);
    res.toolbar = add_toolbar_panel(registry);
    res.keybinds = add_keybinds_panel(registry, "Keybinds");

    // Set up imgui flags

    // Viewport
    {
        auto& viewport_ui = registry.get<UIPanel>(res.viewport);
        viewport_ui.imgui_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        viewport_ui.no_padding = true;
    }

    // Toolbar
    {
        auto& toolbar_ui = registry.get<UIPanel>(res.toolbar);
        toolbar_ui.imgui_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav;
    }

    // Keybinds (hidden, floating)
    {
        auto& keybinds_ui = registry.get<UIPanel>(res.keybinds);
        keybinds_ui.visible = false;
    }


    // Components
    {
        auto& components_ui = registry.get<UIPanel>(res.components);
        components_ui.imgui_flags = ImGuiWindowFlags_AlwaysVerticalScrollbar;
        components_ui.queued_focus = true;
    }

    // Set context variables
    registry.set<DefaultPanels>(res);
    registry.set<FocusedViewportPanel>().viewport_panel = res.viewport;

    return res;
}

void reset_layout(Registry& registry)
{
    constexpr float right_panel_width = 320.0f;
    constexpr float bottom_panel_width = 150.0f;

    const auto ID = registry.ctx<Dockspace>().ID;
    const auto& size = registry.ctx<WindowSize>();
    const auto& default_windows = registry.ctx<DefaultPanels>();

    ImGui::DockBuilderRemoveNode(ID);
    ImGui::DockBuilderAddNode(
        ID,
        ImGuiDockNodeFlagsPrivate_::ImGuiDockNodeFlags_DockSpace |
            ImGuiDockNodeFlagsPrivate_::ImGuiDockNodeFlags_HiddenTabBar);
    ImGui::DockBuilderSetNodeSize(ID, ImVec2(float(size.width), float(size.height)));

    ImGuiID dock_id_main;
    ImGuiID dock_id_right;
    ImGuiID dock_id_right_top;
    ImGuiID dock_id_right_bottom;
    ImGuiID dock_id_bottom;

    ImGui::DockBuilderSplitNode(
        ID,
        ImGuiDir_Right,
        right_panel_width / float(size.width),
        &dock_id_right,
        &dock_id_main);

    ImGui::DockBuilderSplitNode(
        dock_id_right,
        ImGuiDir_Down,
        0.7f,
        &dock_id_right_bottom,
        &dock_id_right_top);

    ImGui::DockBuilderSplitNode(
        dock_id_main,
        ImGuiDir_Down,
        bottom_panel_width / float(size.height),
        &dock_id_bottom,
        &dock_id_main);

    // Disable tab bar for viewport
    {
        ImGuiDockNode* node = ImGui::DockBuilderGetNode(dock_id_main);
        node->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_CentralNode;
    }

    /*
        Dock windows
    */

    if (default_windows.viewport != NullEntity)
        ImGui::DockBuilderDockWindow(
            registry.get<UIPanel>(default_windows.viewport).title.c_str(),
            dock_id_main);

    if (default_windows.scene != NullEntity)
        ImGui::DockBuilderDockWindow(
            registry.get<UIPanel>(default_windows.scene).title.c_str(),
            dock_id_right_top);

    if (default_windows.renderer != NullEntity)
        ImGui::DockBuilderDockWindow(
            registry.get<UIPanel>(default_windows.renderer).title.c_str(),
            dock_id_right_bottom);

    if (default_windows.components != NullEntity)
        ImGui::DockBuilderDockWindow(
            registry.get<UIPanel>(default_windows.components).title.c_str(),
            dock_id_right_bottom);

    if (default_windows.logger != NullEntity)
        ImGui::DockBuilderDockWindow(
            registry.get<UIPanel>(default_windows.logger).title.c_str(),
            dock_id_bottom);

    ImGui::DockBuilderFinish(ID);
}

} // namespace ui
} // namespace lagrange
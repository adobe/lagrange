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
#include <lagrange/ui/components/UIPanel.h>
#include <lagrange/ui/utils/uipanel.h>
#include <lagrange/utils/la_assert.h>
#include <lagrange/utils/strings.h>


#include <imgui.h>

// Docking API is currently internal only
// Change to public API once imgui docking is finished and in master
#include <imgui_internal.h>

namespace lagrange {
namespace ui {


bool begin_panel(UIPanel& window)
{
    if (window.queued_focus && window.imgui_window) {
        ImGui::FocusWindow(window.imgui_window);
        window.queued_focus = false;
    }

    if (window.is_child) {
        return ImGui::BeginChild(
            window.title.c_str(),
            ImVec2(float(window.child_width), float(window.child_height)));
    }


    if (window.no_padding) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    }


    if (window.hide_tab_bar) {
        ImGuiWindowClass cls;
        cls.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_HiddenTabBar;
        ImGui::SetNextWindowClass(&cls);
    }


    bool is_expanded = ImGui::Begin(window.title.c_str(), &window.visible, window.imgui_flags);

    if (window.no_padding) {
        ImGui::PopStyleVar(1);
    }

    window.imgui_window = ImGui::GetCurrentWindow();

    bool focus = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    if (focus != window.is_focused) {
        window.is_focused = focus;
        // Callback here?
    }

    window.is_docked = ImGui::IsWindowDocked();
    if (window.is_docked) {
        window.dock_id = ImGui::GetWindowDockID();
    }

    window.hide_tab_bar = false;

    return is_expanded;
}

void end_panel(UIPanel& window)
{
    if (window.is_child) {
        ImGui::EndChild();
    } else {
        ImGui::End();
    }
}


Entity add_panel(Registry& r, const std::string& title, const std::function<void(void)>& body_fn)
{
    LA_ASSERT(body_fn, "Body function must be set");
    return add_panel(r, title, [=](Registry&, Entity) { body_fn(); });
}


std::string ensure_unique_uiwindow_title(const Registry& r, const std::string& in_title)
{
    auto windows = r.view<const UIPanel>();
    int counter = 1;

    auto title = in_title;

    bool title_exists = false;
    do {
        title_exists = false;
        for (auto e : windows) {
            if (windows.get<const UIPanel>(e).title == title) {
                title_exists = true;
                break;
            }
        }

        if (title_exists) {
            title = lagrange::string_format("{}.{:03d}", title, counter++);
        }

    } while (title_exists);


    return title;
}


Entity add_panel(
    Registry& r,
    const std::string& title,
    const std::function<void(Registry&, Entity)>& body_fn,
    const std::function<void(Registry&, Entity)>& before_fn /*= nullptr*/,
    const std::function<void(Registry&, Entity)>& after_fn /*= nullptr*/, 
    const std::function<void(Registry&, Entity)>& menubar_fn /*= nullptr*/)
{
    auto e = r.create();

    // Initialize UI window component
    auto& uiwindow = r.emplace<UIPanel>(e);
    uiwindow.title = ensure_unique_uiwindow_title(r, title);
    uiwindow.body_fn = body_fn;
    uiwindow.before_fn = before_fn;
    uiwindow.after_fn = after_fn;
    uiwindow.menubar_fn = menubar_fn;

    return e;
}

void toggle_panel(Registry& r, Entity e)
{
    auto& w = r.get<UIPanel>(e);
    w.visible = !w.visible;
}

const WindowSize& get_window_size(const Registry& r)
{
    return r.ctx<WindowSize>();
}

const MainMenuHeight& get_menu_height(const Registry& r)
{
    return r.ctx<MainMenuHeight>();
}

void hide_tab_bar(Registry& r, Entity uipanel_entity)
{
    r.get<UIPanel>(uipanel_entity).hide_tab_bar = true;
}


} // namespace ui
} // namespace lagrange

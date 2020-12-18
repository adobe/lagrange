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
#include <imgui.h>
#include <lagrange/ui/Material.h>
#include <lagrange/ui/Options.h>
#include <lagrange/ui/Texture.h>
#include <lagrange/ui/UIPanel.h>
#include <lagrange/ui/UIWidget.h>
#include <lagrange/ui/Viewer.h>


// Docking API is currently internal only
// Change to public API once imgui docking is finished and in master
#include <imgui_internal.h>



namespace lagrange {
namespace ui {

int UIPanelBase::m_ui_panel_id_counter = 0;

UIPanelBase::UIPanelBase(Viewer* viewer)
: m_viewer(viewer)
, m_ui_panel_id(UIPanelBase::m_ui_panel_id_counter++)
{
    assert(viewer != nullptr);
}

bool UIPanelBase::begin(int flags)
{
    if(!is_child()) {
        bool res = ImGui::Begin(get_title(), &m_visible, flags);

        if(m_enable_tab_bar_active && is_docked()) {
            auto dock = ImGui::GetWindowDockNode();
            if(dock) {
                if(m_enable_tab_bar)
                    dock->LocalFlags &= ~ImGuiDockNodeFlags_NoTabBar;
                else
                    dock->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
            }
        }

        m_imgui_window = ImGui::GetCurrentWindow();

        bool focus = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        if(focus != m_focused) {
            m_focused = focus;
            m_callbacks.call<OnChangeFocus>(*this, focus);
        }

        m_docked = ImGui::IsWindowDocked();
        if(m_docked) {
            m_dock_id = ImGui::GetWindowDockID();
        }

        return res;
    }
    else {
        return ImGui::BeginChild(
            get_title(), ImVec2(float(m_child_width), float(m_child_height)));
    }
}

void UIPanelBase::end()
{
    if(is_child()) {
        ImGui::EndChild();
    }
    else {
        ImGui::End();
    }
}

void UIPanelBase::set_is_child(bool value, int width /*= 0*/, int height /*= 0*/)
{
    m_is_child = value;
    if(value) {
        m_child_width = width;
        m_child_height = height;
    }
}

void UIPanelBase::reset_ui_panel_counter()
{
    UIPanelBase::m_ui_panel_id_counter = 0;
}

int UIPanelBase::get_panel_id() const
{
    return m_ui_panel_id;
}



bool UIPanelBase::is_docked() const
{
    return m_docked;
}

unsigned int UIPanelBase::get_dock_id() const
{
    if(!is_docked()) {
        throw std::logic_error("Panel is not docked");
    }
    return m_dock_id;
}



void UIPanelBase::enable_tab_bar(bool value)
{
    m_enable_tab_bar_active = true;
    m_enable_tab_bar = value;
}

bool UIPanelBase::dock_to(UIPanelBase& target, UIPanelBase::DockDir dir, float ratio, bool split_outer)
{
    auto target_window = target.get_imgui_window();
    if(!target_window) return false;
    if(!target_window->DockNode) return false;

    auto source_window = get_imgui_window();
    if(!source_window) return false;
    if(source_window->DockNode == target_window->DockNode) return false; //already docked

    auto ctx = ImGui::GetCurrentContext();

    ImGui::DockContextQueueDock(
        ctx, target_window, target_window->DockNode, source_window, static_cast<ImGuiDir>(dir), ratio, split_outer);

    return true;
}

bool UIPanelBase::button_toolbar(bool selected,
    const std::string& label,
    const std::string& tooltip,
    const std::string& keybind_id,
    bool enabled)
{
    const float size = ImGui::GetContentRegionAvail().x;
    return button_icon(selected,
        label,
        tooltip,
        keybind_id,
        enabled,
        ImVec2(size, size));
}

bool UIPanelBase::button_icon(bool selected,
    const std::string& label,
    const std::string& tooltip,
    const std::string& keybind_id,
    bool enabled,
    ImVec2 size)
{
    auto& keybinds = get_viewer()->get_keybinds();
    return UIWidget::button_icon(selected, label, tooltip, keybind_id, &keybinds, enabled, size);
}

} // namespace ui
} // namespace lagrange

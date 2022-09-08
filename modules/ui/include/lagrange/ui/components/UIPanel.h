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
#pragma once

#include <lagrange/ui/Entity.h>
#include <Eigen/Eigen>
#include <functional>
#include <string>

struct ImGuiWindow;

namespace lagrange {
namespace ui {


struct UIPanel
{
    enum class DockDir : int {
        AS_NEW_TAB = -1, // Dock as tab
        LEFT = 0,
        RIGHT = 1,
        UP = 2,
        DOWN = 3
    };

    /// Window title, used as ImGUI ID
    std::string title;

    bool visible = true;
    bool is_focused = false;

    int imgui_flags = 0;
    bool no_padding = false;

    bool is_docked = false;
    unsigned int dock_id = 0;
    bool is_child = false;

    int child_width = 0;
    int child_height = 0;

    // set to hide the tab bar. Don't read it, it's only true for one frame.
    bool hide_tab_bar = false;

    bool static_position_enabled = false;
    Eigen::Vector2f static_position;

    bool static_size_enabled = false;
    Eigen::Vector2f static_size;

    ImGuiWindow* imgui_window = nullptr; // non-owning


    bool queued_focus = false;

    std::function<void(Registry&, Entity)> before_fn;
    std::function<void(Registry&, Entity)> body_fn;
    std::function<void(Registry&, Entity)> after_fn;
    std::function<void(Registry&, Entity)> menubar_fn;
};

/*
    Context variables used throughout the UI.
    They are set by the Viewer
*/

struct MainMenuHeight
{
    float height = 0.0f;
};

struct Dockspace
{
    unsigned int ID;
};

struct WindowSize
{
    int width;
    int height;
};


} // namespace ui
} // namespace lagrange
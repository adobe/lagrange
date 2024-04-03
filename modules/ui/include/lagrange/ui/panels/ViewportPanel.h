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

#include <lagrange/ui/api.h>
#include <lagrange/ui/Entity.h>

#include <Eigen/Eigen>

namespace lagrange {
namespace ui {

struct ViewportPanel
{
    Entity viewport;

    int available_width = 0;
    int available_height = 0;

    //Screen position of the canvas/texture being rendered
    Eigen::Vector2f canvas_origin = Eigen::Vector2f::Zero();

    // Is mouse over canvas?
    bool hovered = false;

    // Is any gizmo active in the viewport?
    bool gizmo_active = false;

    // Mouse position in canvas
    Eigen::Vector2f mouse_in_canvas = Eigen::Vector2f::Zero();


    // Show top toolbar
    bool show_viewport_toolbar = true;

    /*
        Helper functions
    */

    /// Returns whether pos is over the viewport
    bool is_over_viewport(const Eigen::Vector2f& pos) const;

    /// Returns position relative to viewport's lower-left corner
    Eigen::Vector2f screen_to_viewport(const Eigen::Vector2f& screen_pos) const;

    /// Returns position relative to the entire screen
    Eigen::Vector2f viewport_to_screen(const Eigen::Vector2f& viewport_pos) const;
};


// Context variable
// At any given time, there must be a viewport window that's in focus
// The actual GUI focus may be a non-viewport window - in that case the focused viewport is the last
// one that was in focus.
struct FocusedViewportPanel
{
    Entity viewport_panel = NullEntity;
};

LA_UI_API Entity add_viewport_panel(Registry& r, const std::string& name, Entity viewport);


} // namespace ui
} // namespace lagrange

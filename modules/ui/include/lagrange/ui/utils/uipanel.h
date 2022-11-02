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

#include <imgui.h>
#include <lagrange/ui/Entity.h>
#include <lagrange/ui/components/UIPanel.h>
#include <lagrange/ui/types/Systems.h>


#include <typeindex>
#include <typeinfo>
#include <unordered_map>

namespace lagrange {
namespace ui {


bool begin_panel(UIPanel& panel);
void end_panel(UIPanel& panel);


/// @brief Adds window that executed given imgui_code. Imgui begin/end is called for you, do not
/// call it inside the imgui_code.
/// @param registry
/// @param title
/// @param imgui_code
/// @return window entity
Entity add_panel(Registry& r, const std::string& title, const std::function<void(void)>& body_fn);

Entity add_panel(
    Registry& r,
    const std::string& title,
    const std::function<void(Registry&, Entity)>& body_fn,
    const std::function<void(Registry&, Entity)>& before_fn = nullptr,
    const std::function<void(Registry&, Entity)>& after_fn = nullptr,
    const std::function<void(Registry&, Entity)>& menubar_fn = nullptr);


void toggle_panel(Registry& r, Entity e);


/// @brief Returns global window size
/// @param registry
/// @return
const WindowSize& get_window_size(const Registry& r);

MainMenuHeight get_menu_height(const Registry& r);


void hide_tab_bar(Registry& r, Entity uipanel_entity);


} // namespace ui
} // namespace lagrange

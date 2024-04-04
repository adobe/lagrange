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
#pragma once

/*
Custom buttons used throughout the UI
*/
#include <lagrange/ui/types/Keybinds.h>
#include <lagrange/ui/api.h>

#include <imgui.h>

namespace lagrange {
namespace ui {

// A plain button with no decoration
LA_UI_API bool button_unstyled(const char* label);

LA_UI_API bool button_toolbar(
    bool selected,
    const std::string& label,
    const std::string& tooltip = "",
    const std::string& keybind_id = "",
    const Keybinds* keybinds = nullptr,
    bool enabled = true);

LA_UI_API bool button_icon(
    bool selected,
    const std::string& label,
    const std::string& tooltip = "",
    const std::string& keybind_id = "",
    const Keybinds* keybinds = nullptr,
    bool enabled = true,
    ImVec2 size = ImVec2(0, 0));

} // namespace ui
} // namespace lagrange

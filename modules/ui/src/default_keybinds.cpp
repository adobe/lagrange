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
#include <lagrange/ui/default_keybinds.h>
#include <lagrange/ui/types/GLContext.h>


#include <unordered_map>

namespace lagrange {
namespace ui {

using Scheme = std::unordered_map<std::string, std::pair<ImGuiKey, std::vector<ImGuiKey>>>;


const static std::unordered_map<DefaultCameraScheme, Scheme> default_schemes = {
    {DefaultCameraScheme::DIMENSION,
     {{"viewport.camera_rotate", {ImGuiKey_MouseRight, {}}},
      {"viewport.camera_pan", {ImGuiKey_MouseMiddle, {}}},
      {"viewport.camera_pan", {ImGuiKey_MouseRight, {ImGuiKey_Space}}},
      {"viewport.camera_dolly", {ImGuiKey_MouseRight, {ImGuiKey_Space, ImGuiKey_LeftShift}}},
      {"viewport.camera_center_on_cursor", {ImGuiKey_X, {}}},
      {"camera_center_on_selection", {ImGuiKey_Z, {}}}}},

    {DefaultCameraScheme::MAYA,
     {{"viewport.camera_rotate", {ImGuiKey_MouseLeft, {ImGuiKey_LeftAlt}}},
      {"viewport.camera_pan", {ImGuiKey_MouseMiddle, {ImGuiKey_LeftAlt}}},
      {"viewport.camera_dolly", {ImGuiKey_MouseRight, {ImGuiKey_LeftAlt}}},
      {"viewport.camera_center_on_cursor", {ImGuiKey_X, {ImGuiKey_LeftAlt}}},
      {"camera_center_on_selection", {ImGuiKey_Z, {ImGuiKey_LeftAlt}}},
      {"viewport.selection_select_erase",
       {ImGuiKey_MouseLeft, {ImGuiKey_LeftCtrl, ImGuiKey_LeftAlt}}}}},


    {DefaultCameraScheme::BLENDER,
     {{"viewport.camera_rotate", {ImGuiKey_MouseMiddle, {}}},
      {"viewport.camera_pan", {ImGuiKey_MouseMiddle, {ImGuiKey_LeftShift}}},
      {"viewport.camera_dolly", {ImGuiKey_MouseMiddle, {ImGuiKey_LeftCtrl}}},
      {"viewport.camera_center_on_cursor", {ImGuiKey_MouseMiddle, {ImGuiKey_LeftAlt}}},
      {"camera_center_on_selection", {ImGuiKey_Z, {}}}}},

    {DefaultCameraScheme::SUBSTANCE,
     {{"viewport.camera_rotate", {ImGuiKey_MouseLeft, {ImGuiKey_LeftAlt}}},
      {"viewport.camera_pan", {ImGuiKey_MouseMiddle, {ImGuiKey_LeftAlt}}},
      {"viewport.camera_dolly", {ImGuiKey_MouseRight, {ImGuiKey_LeftAlt}}},
      {"viewport.camera_center_on_cursor", {ImGuiKey_F, {ImGuiKey_LeftAlt}}},
      {"camera_center_on_selection", {ImGuiKey_F, {}}},
      {"viewport.selection_select_erase",
       {ImGuiKey_MouseLeft, {ImGuiKey_LeftCtrl, ImGuiKey_LeftAlt}}}}}};

void set_camera_scheme(Keybinds& keybinds, DefaultCameraScheme camera_scheme)
{
    const auto& scheme = default_schemes.at(camera_scheme);

    // Remove all keybinds, they are going to be rewritten
    for (auto& it : scheme) {
        keybinds.remove(it.first);
    }

    // Add scheme keys
    for (auto& it : scheme) {
        keybinds.add(it.first, it.second.first, it.second.second);
    }
}

Keybinds initialize_default_keybinds()
{
    Keybinds k;
    k.add("manipulation_mode_select", ImGuiKey_Q);
    k.add("manipulation_mode_translate", ImGuiKey_W);
    k.add("manipulation_mode_rotate", ImGuiKey_E);
    k.add("manipulation_mode_scale", ImGuiKey_R);

    k.add("selection_mode_object", ImGuiKey_Q, {ImGuiKey_LeftAlt});
    k.add("selection_mode_face", ImGuiKey_W, {ImGuiKey_LeftAlt});
    k.add("selection_mode_edge", ImGuiKey_E, {ImGuiKey_LeftAlt});
    k.add("selection_mode_vertex", ImGuiKey_R, {ImGuiKey_LeftAlt});

    k.register_action("selection_mode_select_backfaces");
    k.register_action("selection_mode_paint_selection");

    k.add("viewport.selection_select_set", ImGuiKey_MouseLeft);
    k.add("viewport.selection_select_add", ImGuiKey_MouseLeft, {ImGuiKey_LeftCtrl});
    k.add("viewport.selection_select_erase", ImGuiKey_MouseLeft, {ImGuiKey_LeftAlt});

    k.add("scene_open", ImGuiKey_O, {ImGuiKey_LeftCtrl});
    k.add("scene_add", ImGuiKey_O, {ImGuiKey_LeftCtrl, ImGuiKey_LeftShift});

    k.add("camera_zoom_to_fit", ImGuiKey_Z, {ImGuiKey_LeftShift});

    k.add("reload", ImGuiKey_R, {ImGuiKey_LeftShift});

    set_camera_scheme(k, DefaultCameraScheme::DIMENSION);

    return k;
}

bool has_camera_scheme(const Keybinds& keybinds, DefaultCameraScheme camera_scheme)
{
    const auto& scheme = default_schemes.at(camera_scheme);

    auto map = keybinds.get();

    for (auto& it : scheme) {
        if (!keybinds.has(it.first, it.second.first, it.second.second)) return false;
    }

    return true;
}
} // namespace ui
} // namespace lagrange

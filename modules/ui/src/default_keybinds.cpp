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

using Scheme = std::unordered_map<std::string, std::pair<int, std::vector<int>>>;


const static std::unordered_map<DefaultCameraScheme, Scheme> default_schemes = {
    {DefaultCameraScheme::DIMENSION,
     {{"viewport.camera_rotate", {GLFW_MOUSE_BUTTON_2, {}}},
      {"viewport.camera_pan", {GLFW_MOUSE_BUTTON_3, {}}},
      {"viewport.camera_pan", {GLFW_MOUSE_BUTTON_2, {GLFW_KEY_SPACE}}},
      {"viewport.camera_dolly", {GLFW_MOUSE_BUTTON_2, {GLFW_KEY_SPACE, GLFW_KEY_LEFT_SHIFT}}},
      {"viewport.camera_center_on_cursor", {GLFW_KEY_X, {}}},
      {"camera_center_on_selection", {GLFW_KEY_Z, {}}}}},

    {DefaultCameraScheme::MAYA,
     {{"viewport.camera_rotate", {GLFW_MOUSE_BUTTON_1, {GLFW_KEY_LEFT_ALT}}},
      {"viewport.camera_pan", {GLFW_MOUSE_BUTTON_3, {GLFW_KEY_LEFT_ALT}}},
      {"viewport.camera_dolly", {GLFW_MOUSE_BUTTON_2, {GLFW_KEY_LEFT_ALT}}},
      {"viewport.camera_center_on_cursor", {GLFW_KEY_X, {GLFW_KEY_LEFT_ALT}}},
      {"camera_center_on_selection", {GLFW_KEY_Z, {GLFW_KEY_LEFT_ALT}}},
      {"viewport.selection_select_erase",
       {GLFW_MOUSE_BUTTON_1, {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_LEFT_ALT}}}}},


    {DefaultCameraScheme::BLENDER,
     {{"viewport.camera_rotate", {GLFW_MOUSE_BUTTON_3, {}}},
      {"viewport.camera_pan", {GLFW_MOUSE_BUTTON_3, {GLFW_KEY_LEFT_SHIFT}}},
      {"viewport.camera_dolly", {GLFW_MOUSE_BUTTON_3, {GLFW_KEY_LEFT_CONTROL}}},
      {"viewport.camera_center_on_cursor", {GLFW_MOUSE_BUTTON_3, {GLFW_KEY_LEFT_ALT}}},
      {"camera_center_on_selection", {GLFW_KEY_Z, {}}}}},

    {DefaultCameraScheme::SUBSTANCE,
     {{"viewport.camera_rotate", {GLFW_MOUSE_BUTTON_1, {GLFW_KEY_LEFT_ALT}}},
      {"viewport.camera_pan", {GLFW_MOUSE_BUTTON_3, {GLFW_KEY_LEFT_ALT}}},
      {"viewport.camera_dolly", {GLFW_MOUSE_BUTTON_2, {GLFW_KEY_LEFT_ALT}}},
      {"viewport.camera_center_on_cursor", {GLFW_KEY_F, {GLFW_KEY_LEFT_ALT}}},
      {"camera_center_on_selection", {GLFW_KEY_F, {}}},
      {"viewport.selection_select_erase",
       {GLFW_MOUSE_BUTTON_1, {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_LEFT_ALT}}}}}};

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
    k.add("manipulation_mode_select", GLFW_KEY_Q);
    k.add("manipulation_mode_translate", GLFW_KEY_W);
    k.add("manipulation_mode_rotate", GLFW_KEY_E);
    k.add("manipulation_mode_scale", GLFW_KEY_R);

    k.add("selection_mode_object", GLFW_KEY_Q, {GLFW_KEY_LEFT_ALT});
    k.add("selection_mode_face", GLFW_KEY_W, {GLFW_KEY_LEFT_ALT});
    k.add("selection_mode_edge", GLFW_KEY_E, {GLFW_KEY_LEFT_ALT});
    k.add("selection_mode_vertex", GLFW_KEY_R, {GLFW_KEY_LEFT_ALT});

    k.register_action("selection_mode_select_backfaces");
    k.register_action("selection_mode_paint_selection");

    k.add("viewport.selection_select_set", GLFW_MOUSE_BUTTON_1);
    k.add("viewport.selection_select_add", GLFW_MOUSE_BUTTON_1, {GLFW_KEY_LEFT_CONTROL});
    k.add("viewport.selection_select_erase", GLFW_MOUSE_BUTTON_1, {GLFW_KEY_LEFT_ALT});

    k.add("scene_open", GLFW_KEY_O, {GLFW_KEY_LEFT_CONTROL});
    k.add("scene_add", GLFW_KEY_O, {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_LEFT_SHIFT});

    k.add("camera_zoom_to_fit", GLFW_KEY_Z, {GLFW_KEY_LEFT_SHIFT});

    k.add("reload", GLFW_KEY_R, {GLFW_KEY_LEFT_SHIFT});

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

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
#include <lagrange/ui/types/GLContext.h>
#include <lagrange/ui/default_keybinds.h>


#include <unordered_map>

namespace lagrange {
namespace ui {

using Scheme = std::unordered_map<std::string, std::pair<int, std::vector<int>>>;


const static std::unordered_map<DefaultCameraScheme, Scheme> default_schemes = {
    {DefaultCameraScheme::DIMENSION,
     {{"viewport.camera.rotate", {GLFW_MOUSE_BUTTON_2, {}}},
      {"viewport.camera.pan", {GLFW_MOUSE_BUTTON_3, {}}},
      {"viewport.camera.pan", {GLFW_MOUSE_BUTTON_2, {GLFW_KEY_SPACE}}},
      {"viewport.camera.dolly", {GLFW_MOUSE_BUTTON_2, {GLFW_KEY_SPACE, GLFW_KEY_LEFT_SHIFT}}},
      {"viewport.camera.center_on_cursor", {GLFW_KEY_X, {}}},
      {"global.camera.center_on_selection", {GLFW_KEY_Z, {}}}}},

    {DefaultCameraScheme::MAYA,
     {{"viewport.camera.rotate", {GLFW_MOUSE_BUTTON_1, {GLFW_KEY_LEFT_ALT}}},
      {"viewport.camera.pan", {GLFW_MOUSE_BUTTON_3, {GLFW_KEY_LEFT_ALT}}},
      {"viewport.camera.dolly", {GLFW_MOUSE_BUTTON_2, {GLFW_KEY_LEFT_ALT}}},
      {"viewport.camera.center_on_cursor", {GLFW_KEY_X, {GLFW_KEY_LEFT_ALT}}},
      {"global.camera.center_on_selection", {GLFW_KEY_Z, {GLFW_KEY_LEFT_ALT}}},
      {"viewport.selection.select.erase",
       {GLFW_MOUSE_BUTTON_1, {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_LEFT_ALT}}}}},


    {DefaultCameraScheme::BLENDER,
     {{"viewport.camera.rotate", {GLFW_MOUSE_BUTTON_3, {}}},
      {"viewport.camera.pan", {GLFW_MOUSE_BUTTON_3, {GLFW_KEY_LEFT_SHIFT}}},
      {"viewport.camera.dolly", {GLFW_MOUSE_BUTTON_3, {GLFW_KEY_LEFT_CONTROL}}},
      {"viewport.camera.center_on_cursor", {GLFW_MOUSE_BUTTON_3, {GLFW_KEY_LEFT_ALT}}},
      {"global.camera.center_on_selection", {GLFW_KEY_Z, {}}}}},

    {DefaultCameraScheme::SUBSTANCE,
     {{"viewport.camera.rotate", {GLFW_MOUSE_BUTTON_1, {GLFW_KEY_LEFT_ALT}}},
      {"viewport.camera.pan", {GLFW_MOUSE_BUTTON_3, {GLFW_KEY_LEFT_ALT}}},
      {"viewport.camera.dolly", {GLFW_MOUSE_BUTTON_2, {GLFW_KEY_LEFT_ALT}}},
      {"viewport.camera.center_on_cursor", {GLFW_KEY_F, {GLFW_KEY_LEFT_ALT}}},
      {"global.camera.center_on_selection", {GLFW_KEY_F, {}}},
      {"viewport.selection.select.erase",
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
    k.add("global.manipulation_mode.select", GLFW_KEY_Q);
    k.add("global.manipulation_mode.translate", GLFW_KEY_W);
    k.add("global.manipulation_mode.rotate", GLFW_KEY_E);
    k.add("global.manipulation_mode.scale", GLFW_KEY_R);

    k.add("global.selection_mode.object", GLFW_KEY_Q, {GLFW_KEY_LEFT_ALT});
    k.add("global.selection_mode.face", GLFW_KEY_W, {GLFW_KEY_LEFT_ALT});
    k.add("global.selection_mode.edge", GLFW_KEY_E, {GLFW_KEY_LEFT_ALT});
    k.add("global.selection_mode.vertex", GLFW_KEY_R, {GLFW_KEY_LEFT_ALT});

    k.register_action("global.selection_mode.select_backfaces");
    k.register_action("global.selection_mode.paint_selection");

    k.add("viewport.selection.select.set", GLFW_MOUSE_BUTTON_1);
    k.add("viewport.selection.select.add", GLFW_MOUSE_BUTTON_1, {GLFW_KEY_LEFT_CONTROL});
    k.add("viewport.selection.select.erase", GLFW_MOUSE_BUTTON_1, {GLFW_KEY_LEFT_ALT});

    k.add("global.renderer.reset", GLFW_KEY_R, {GLFW_KEY_LEFT_SHIFT});

    k.add("global.scene.open", GLFW_KEY_O, {GLFW_KEY_LEFT_CONTROL});
    k.add("global.scene.add", GLFW_KEY_O, {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_LEFT_SHIFT});

    k.add("global.camera.zoom_to_fit", GLFW_KEY_Z, {GLFW_KEY_LEFT_SHIFT});

    k.add("global.reload", GLFW_KEY_R, {GLFW_KEY_LEFT_SHIFT});

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

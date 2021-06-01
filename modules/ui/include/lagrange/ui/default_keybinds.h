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
#include <lagrange/ui/types/Keybinds.h>

namespace lagrange {
namespace ui {

///
/// Default camera scheme based on other software
/// To create your own,
/// \see set_camera_scheme for which actions are available
///
enum class DefaultCameraScheme { DIMENSION, MAYA, BLENDER, SUBSTANCE };

///
/// Set camera control scheme from DefaultCameraScheme
///
/// Binds following actions:
/// camera.select, camera.rotate,
/// camera.pan, camera.dolly,
/// camera.center_on_cursor, camera.center_on_selection
///
/// In case of MAYA and SUBSTANCE, they remap viewport.selection.select.erase to
/// LEFT MOUSE + LEFT CTRL + LEFT ALT
///
void set_camera_scheme(Keybinds& keybinds, DefaultCameraScheme camera_scheme);


///
/// Do the keybinds have one of the default camera schemes?
///
bool has_camera_scheme(const Keybinds& keybinds, DefaultCameraScheme camera_scheme);


///
/// Initializes all default keybinds
///
Keybinds initialize_default_keybinds();


} // namespace ui
} // namespace lagrange

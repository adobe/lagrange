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

#include <lagrange/ui/Entity.h>

namespace lagrange {
namespace ui {

/// Adjust camera based on CameraController
void camera_controller_system(Registry& registry);


/// Rotates Camera based on CameraTurntable component
void camera_turntable_system(Registry& registry);


/// Zooms Camera based on CameraZoomToFit component
void camera_focusfit_system(Registry& registry);


} // namespace ui
} // namespace lagrange
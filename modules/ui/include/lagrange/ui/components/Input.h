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
#include <Eigen/Eigen>

namespace lagrange {
namespace ui {

struct InputState
{
    std::shared_ptr<Keybinds> keybinds;

    struct Mouse
    {
        /// Position in pixels from top-left of the viewer window
        Eigen::Vector2f position = Eigen::Vector2f(0, 0);

        /// Change in mouse pixel position from last frame
        Eigen::Vector2f delta = Eigen::Vector2f(0, 0);

        /// Vertical wheel change during this frame
        float wheel = 0.0f;

        /// Horizontal wheel change during this frame
        float wheel_horizontal = 0.0f;
    } mouse;
};

} // namespace ui
} // namespace lagrange

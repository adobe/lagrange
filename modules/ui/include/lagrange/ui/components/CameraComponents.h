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

namespace lagrange {
namespace ui {


struct CameraController
{
    bool any_control_active = false;

    // Viewport state
    Eigen::Vector2f mouse_current = Eigen::Vector2f::Zero();
    Eigen::Vector2f mouse_delta = Eigen::Vector2f::Zero();

    // Rotation
    bool rotation_active = false;
    Eigen::Vector2f rotation_mouse_start = Eigen::Vector2f::Zero();
    Eigen::Vector3f rotation_camera_pos_start = Eigen::Vector3f::Zero();
    Eigen::Vector3f rotation_camera_up_start = Eigen::Vector3f::Zero();

    //Zoom/dolly
    bool dolly_active = false;
    float dolly_delta = 0.0f;
    Eigen::Vector2f dolly_mouse_start = Eigen::Vector2f::Zero();

    // Panning
    bool pan_active = false;
    float pan_speed = 0.0005f;

    // Other options
    bool ortho_interaction_2D = false;
    bool auto_nearfar = true;
    bool fov_zoom = false;
};


struct CameraTurntable
{
    bool enabled = false;
    float speed = 1.0f / 4.0f;
    Eigen::Vector3f start_pos = Eigen::Vector3f::Zero();
    Eigen::Vector3f axis = Eigen::Vector3f(0, 1, 0);
};

struct CameraFocusAndFit
{
    std::function<bool(Registry& r, Entity e)> filter = nullptr;
    bool focus = true;
    bool fit = false;
    float current_time = 0.0f;
    float speed = 0.25f;
};


} // namespace ui
} // namespace lagrange

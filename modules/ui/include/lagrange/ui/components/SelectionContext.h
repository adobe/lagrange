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
#include <lagrange/ui/components/Selection.h>
#include <lagrange/ui/types/Camera.h>
#include <lagrange/ui/types/Frustum.h>
#include <Eigen/Eigen>

namespace lagrange {
namespace ui {

/// Global component with information about current viewport selection
struct SelectionContext
{
    // Registered element type using register_tool_type
    entt::id_type element_type;

    /// In which viewport is the selection happening
    /// Can be null or invalid if no viewport is hovered
    Entity active_viewport = NullEntity;

    /// Is selection active (are the selection keys down?)
    bool active = false;

    /// Is marquee (rectangle) selection active
    bool marquee_active = false;

    /*
        Screen space
    */

    /// Beginning of selection rectangle (coordinates can be greater than end)
    Eigen::Vector2f screen_begin;

    /// End of selection rectangle (coordinates can be lower than begin)
    Eigen::Vector2f screen_end;

    /// Current selection position
    Eigen::Vector2f screen_position;

    /*
        Viewport space (clipped)
    */

    /// Beginning of selection rectangle (always lower coordinate than end)
    Eigen::Vector2i viewport_min;

    /// End of selection rectangle (always greater coordinate than begin)
    Eigen::Vector2i viewport_max;

    /// Current selection position
    Eigen::Vector2i viewport_position;


    /// Current mouse position induced ray origin
    Eigen::Vector3f ray_origin;

    /// Current mouse position induced ray direction
    Eigen::Vector3f ray_dir;

    /// Frustum, set if marquee is active
    Frustum frustum;

    /// Frustum in a radius
    Frustum neighbourhood_frustum;
    float neighbourhood_frustum_radius = 10.0f; // px

    /// Frustum in one pixel diameter
    Frustum onepx_frustum;

    /// Copy of current camera
    Camera camera;

    /// Selection behavior
    SelectionBehavior behavior = SelectionBehavior::SET;

    /// Select backfacing objects/elements
    bool select_backfacing = false;
};


} // namespace ui
} // namespace lagrange
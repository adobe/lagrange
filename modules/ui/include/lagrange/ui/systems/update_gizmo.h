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
#include <lagrange/ui/api.h>
#include <lagrange/ui/components/Transform.h>
#include <lagrange/ui/types/Camera.h>


namespace lagrange {
namespace ui {

enum class GizmoMode { SELECT, TRANSLATE, ROTATE, SCALE, count };

/// Gizmo system state (context variable)
struct GizmoContext
{
    bool active = false;
    Eigen::Affine3f current_transform = Eigen::Affine3f::Identity();
    Eigen::Affine3f transform_start = Eigen::Affine3f::Identity();
};

/// GizmoObjectTransform component is attached to every entity being
/// transformed by the gizmo system
struct GizmoObjectTransform
{
    /// Transform of the object at the start of gizmo interaction
    ui::Transform initial_transform;

    /// Cached parent inverse
    /// Computed as (global * local.inverse()).inverse()
    Eigen::Affine3f parent_inverse;
};


LA_UI_API bool gizmo_system_is_using();
LA_UI_API bool gizmo_system_is_over();
LA_UI_API void gizmo_system_set_draw_list();

LA_UI_API void gizmo_system(
    Registry& registry,
    const Camera& camera,
    const Eigen::Vector2f& canvas_pos,
    GizmoMode mode = GizmoMode::SELECT);


} // namespace ui
} // namespace lagrange

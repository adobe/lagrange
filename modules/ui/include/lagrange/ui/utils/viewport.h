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

#include <lagrange/ui/components/Viewport.h>
#include <lagrange/ui/panels/ViewportPanel.h>

namespace lagrange {
namespace ui {

/// Creates an offscreen viewport with given camera. Create ViewportPanel to show it on screen.
Entity add_viewport(Registry& registry, Entity camera_entity, bool srgb = false);

void instance_camera_to_viewports(Registry& registry, Entity source_viewport);
void copy_camera_to_viewports(Registry& registry, Entity source_viewport);

/*
    Focused Viewport
*/

/// Focused Viewport UI Panel
/// Returns nullptr if there is no focused viewport
ViewportPanel* get_focused_viewport_panel(Registry& registry);

/// Focused Viewport (entity)
/// Returns NullEntity if there is no focused viewport
Entity get_focused_viewport_entity(Registry& registry);

/// Focused Viewport (component reference)
/// Returns nullptr if there is no focused viewport
ViewportComponent* get_focused_viewport(Registry& registry);


/*
    Focused Camera
*/
// Focused Camera (entity)
/// Returns NullEntity if there is no focused viewport
Entity get_focused_camera_entity(Registry& registry);

// Focused Camera (component reference)
/// Returns nullptr if there is no focused viewport
Camera* get_focused_camera(Registry& registry);

// Camera component of entity e
Camera& get_camera(Registry& registry, Entity e);

/*
    Hovered Viewport
*/

// Hovered Viewport UI Panel (entity)
/// Returns NullEntity if there is no hovered viewport
Entity get_hovered_viewport_panel_entity(Registry& registry);

// Hovered Viewport (entity)
/// Returns NullEntity if there is no hovered viewport
Entity get_hovered_viewport_entity(Registry& registry);




/// @brief Adjusts camera to fit the scene bounding box over the next several frames.
/// If filter is specified, it will focus only on entities passing the filter test.
/// @param registry
/// @param camera entity with a Camera component
/// @param focus adjusts "lookat" position of the camera
/// @param fit  adjusts "zoom" or "fov" of the camera to fit the bounding box
/// @param duration_seconds duration in seconds
/// @param filter
/// Returns false if camera is not a valid entity
bool camera_focus_and_fit(
    Registry& registry,
    Entity camera,
    bool focus = true,
    bool fit = true,
    float duration_seconds = 1.0f,
    const std::function<bool(Registry& r, Entity e)>& filter = nullptr);

/// @brief Adjusts focused camera to fit the scene bounding box over the next several frames.
void camera_focus_and_fit(Registry& registry);

/*
    Internal offscreen viewport utilities
*/
Entity get_selection_viewport_entity(const Registry& registry);
Entity get_objectid_viewport_entity(const Registry& registry);
void add_selection_outline_post_process(Registry& registry, Entity viewport_entity);


} // namespace ui
} // namespace lagrange
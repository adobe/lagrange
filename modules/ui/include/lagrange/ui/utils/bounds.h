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
#include <lagrange/ui/components/Bounds.h>
#include <lagrange/ui/components/Layer.h>

namespace lagrange {
namespace ui {


/// Returns Axis Aligned Bounding Box of the entity in world space
/// If entity does not have bounds, returns an empty AABB
LA_UI_API AABB get_bounding_box(const Registry& registry, Entity e);

/// Returns Axis Aligned Bounding Box of the entity in model space
/// If entity does not have bounds, returns an empty AABB
LA_UI_API AABB get_bounding_box_local(const Registry& registry, Entity e);

/// Returns Axis Aligned Bounding Box of all entities with <Selected> component
/// If there's no selection returns an empty AABB
LA_UI_API AABB get_selection_bounding_box(const Registry& registry);

/// Returns the least distance between `from` and any point within any bounding box.
/// Returns 0 if `from` lies within a bounding box.
/// Returns -1 if no bounds exist.
LA_UI_API float get_nearest_bounds_distance(
    const Registry& registry,
    const Eigen::Vector3f& from,
    const Layer& visible,
    const Layer& hidden);

/// Returns the greatest distance between `from` and any point within any bounding box.
/// Returns -1 if no bounds exist.
LA_UI_API float get_furthest_bounds_distance(
    const Registry& registry,
    const Eigen::Vector3f& from,
    const Layer& visible,
    const Layer& hidden);

/// Returns the bounding box of everything
/// (must be set as context variable after update_scene_bounds)
LA_UI_API AABB get_scene_bounding_box(const Registry& registry);

/// Returns the bounds of everything
/// (must be set as context variable after update_scene_bounds)
LA_UI_API const Bounds& get_scene_bounds(const Registry& registry);

/// @copydoc
LA_UI_API Bounds& get_scene_bounds(Registry& registry);


} // namespace ui
} // namespace lagrange

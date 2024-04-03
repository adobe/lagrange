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

#include <lagrange/ui/api.h>
#include <lagrange/ui/Entity.h>
#include <lagrange/ui/types/Frustum.h>
#include <lagrange/ui/utils/mesh.h>


namespace lagrange {
namespace ui {

struct MeshSelectionRender;


/// Mesh triangle X ray intersection
/// Uses an accelerated data structure if enabled for entity e (use `enable_accelerated_picking`)
LA_UI_API std::optional<RayFacetHit>
intersect_ray(Registry& r, Entity e, const Eigen::Vector3f& origin, const Eigen::Vector3f& dir);

/*
    Element selection
*/

/// @brief Selects visible mesh elements (sets `is_selected` attribute)
/// @param r Registry
/// @param element_type type of element (ElementFace, ElementEdge, ElementVertex)
/// @param sel_behavior SelectionBehavior (SET/ADD/ERASE)
/// @param selected_entity Entity with MeshGeometry component
/// @param active_viewport Viewport to use for resolution and layer visibility
/// @param local_frustum Frustum in mesh's local coordinate frame
/// @return always true
LA_UI_API bool select_visible_elements(
    Registry& r,
    StringID element_type,
    SelectionBehavior sel_behavior,
    Entity selected_entity,
    Entity active_viewport,
    Frustum local_frustum);

/// @brief Selects mesh elements intersecting a frustum (sets `is_selected` attribute)
/// @param r
/// @param element_type type of element (ElementFace, ElementEdge, ElementVertex)
/// @param sel_behavior SelectionBehavior (SET/ADD/ERASE)
/// @param selected_entity Entity with MeshGeometry component
/// @param local_frustum Frustum in mesh's local coordinate frame
/// @return always true
LA_UI_API bool select_elements_in_frustum(
    Registry& r,
    StringID element_type,
    SelectionBehavior sel_behavior,
    Entity selected_entity,
    Frustum local_frustum);

/*
    Mesh element selection visualization utilities
*/

LA_UI_API void clear_element_selection_render(Registry& r, bool exclude_selected);

LA_UI_API MeshSelectionRender& ensure_selection_render(Registry& r, Entity e);

/// Update materials and visibility of different mesh elements
LA_UI_API void update_selection_render(
    Registry& r,
    MeshSelectionRender& sel_render,
    const Entity& selected_mesh_entity,
    const StringID& current_element_type);

LA_UI_API void mark_selection_dirty(Registry& r, MeshSelectionRender& sel_render);

/*
    Accelerated picking
*/

/// Computes acceleration structure (igl::AABB) for faster ray-triangle intersection
/// Entity can be entity with MeshGeometry or MeshData
/// Returns false if entity does not have a mesh
LA_UI_API bool enable_accelerated_picking(Registry& r, Entity e);

LA_UI_API bool has_accelerated_picking(Registry& r, Entity e);


} // namespace ui
} // namespace lagrange

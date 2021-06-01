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

#include <lagrange/ui/systems/update_mesh_elements_hovered.h>

#include <lagrange/Logger.h>
#include <lagrange/attributes/attribute_utils.h>
#include <lagrange/ui/components/AttributeRender.h>
#include <lagrange/ui/components/Input.h>
#include <lagrange/ui/components/MeshGeometry.h>
#include <lagrange/ui/components/MeshSelectionRender.h>
#include <lagrange/ui/components/SelectionContext.h>
#include <lagrange/ui/components/Transform.h>
#include <lagrange/ui/default_tools.h>
#include <lagrange/ui/utils/layer.h>
#include <lagrange/ui/utils/mesh.h>
#include <lagrange/ui/utils/mesh_picking.h>
#include <lagrange/ui/utils/selection.h>
#include <lagrange/ui/utils/viewport.h>

namespace lagrange {
namespace ui {

Entity ensure_single_selection(Registry& r)
{
    auto v = r.view<MeshGeometry, Selected, Transform>();
    const auto sel_count = std::count_if(v.begin(), v.end(), [](const auto&) { return true; });
    if (sel_count == 0) return NullEntity;

    const auto e = *v.begin();
    if (sel_count > 1) ui::set_selected(r, e, SelectionBehavior::SET);

    return e;
}


void update_mesh_elements_hovered(Registry& r)
{
    const auto& sel_ctx = get_selection_context(r);


    // Clear any unused SelectionRender entities
    if (!is_element_type<ElementFace>(sel_ctx.element_type) &&
        !is_element_type<ElementEdge>(sel_ctx.element_type) &&
        !is_element_type<ElementVertex>(sel_ctx.element_type)) {
        // Keep the one currently selected
        clear_element_selection_render(r, false);
        return;
    } else {
        // Remove all
        clear_element_selection_render(r, true);
    }

    const Entity e = ensure_single_selection(r);
    if (!r.valid(e)) return;


    // Setup element selection visualization
    auto& sel_render = ensure_selection_render(r, e);
    update_selection_render(r, sel_render, e, sel_ctx.element_type);


    // Ensure "is_selected" attribute is allocated
    const auto geom_entity = r.get<MeshGeometry>(e).entity;
    auto& mesh_data = get_mesh_data(r, e);
    ensure_is_selected_attribute(mesh_data);

    if (!r.valid(sel_ctx.active_viewport)) return;
    if (!are_selection_keys_released(r)) return;

    
    const auto& transform = r.get<Transform>(e).global;
    const auto invT = transform.inverse();

    bool has_changed = false;
    /*
        Marquee/Rectangle selection
    */
    if (sel_ctx.marquee_active) {
        auto local_frustum = sel_ctx.frustum.transformed(invT);

        // Visible elements only, use offscreen GL viewport
        if (!sel_ctx.select_backfacing) {
            has_changed = select_visible_elements(
                r,
                sel_ctx.element_type,
                sel_ctx.behavior,
                e,
                sel_ctx.active_viewport,
                local_frustum);
        }
        // All elements in frustum, use parallel frustum intersection
        else {
            has_changed = select_elements_in_frustum(
                r,
                sel_ctx.element_type,
                sel_ctx.behavior,
                e,
                local_frustum);
        }
    } else {
        
        const auto attrib_name = "is_selected";

        const Camera::Ray local_ray = {invT * sel_ctx.ray_origin, invT.linear() * sel_ctx.ray_dir};


        if (is_element_type<ElementVertex>(sel_ctx.element_type)) {
            auto local_frustum = sel_ctx.neighbourhood_frustum.transformed(invT);
            select_vertices_in_frustum(mesh_data, sel_ctx.behavior, local_frustum);
            filter_closest_vertex(
                mesh_data,
                attrib_name,
                sel_ctx.behavior,
                sel_ctx.camera,
                sel_ctx.viewport_position);
            propagate_vertex_selection(mesh_data, attrib_name);
            has_changed = true;

        } else if (is_element_type<ElementFace>(sel_ctx.element_type)) {
            std::optional<RayFacetHit> res = {};

            res = intersect_ray(r, geom_entity, local_ray.origin, local_ray.dir);

            if (res) {
                select_facets(mesh_data, sel_ctx.behavior, {res->facet_id});
                propagate_facet_selection(mesh_data, attrib_name);
                has_changed = true;
            }
        } else if (is_element_type<ElementEdge>(sel_ctx.element_type)) {
            // TODO: improve. Not precise, can select more than one edge
            auto local_frustum = sel_ctx.neighbourhood_frustum.transformed(invT);
            has_changed = select_visible_elements(
                r,
                sel_ctx.element_type,
                sel_ctx.behavior,
                e,
                sel_ctx.active_viewport,
                local_frustum);
        }
    }

    if (has_changed) {
        mark_selection_dirty(r, sel_render);
    }
}

} // namespace ui
} // namespace lagrange

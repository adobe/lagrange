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

#include <lagrange/Logger.h>
#include <lagrange/ui/types/Tools.h>
#include <lagrange/ui/utils/objectid_viewport.h>

#include <lagrange/ui/components/Bounds.h>
#include <lagrange/ui/components/MeshGeometry.h>
#include <lagrange/ui/components/MeshRender.h>
#include <lagrange/ui/components/SelectionContext.h>
#include <lagrange/ui/components/Transform.h>
#include <lagrange/ui/default_shaders.h>
#include <lagrange/ui/default_tools.h>
#include <lagrange/ui/panels/ViewportPanel.h>
#include <lagrange/ui/systems/render_viewports.h>
#include <lagrange/ui/systems/update_mesh_hovered.h>
#include <lagrange/ui/types/Material.h>
#include <lagrange/ui/types/Shader.h>
#include <lagrange/ui/utils/layer.h>
#include <lagrange/ui/utils/selection.h>
#include <lagrange/ui/utils/viewport.h>


namespace lagrange {
namespace ui {

struct HoveredTemp
{
    bool _dummy;
};

void update_mesh_hovered_GL(Registry& r, const SelectionContext& sel_ctx)
{
    if (!r.valid(sel_ctx.active_viewport)) {
        return;
    }

    auto& offscreen_viewport = setup_offscreen_viewport(
        r,
        get_objectid_viewport_entity(r),
        sel_ctx.active_viewport,
        DefaultShaders::ObjectID);
    auto [x, y, w, h] = setup_scissor(r, offscreen_viewport, sel_ctx);

    // Select only visible -> use color read back
    if (!sel_ctx.select_backfacing) {
        const auto pixel_num = w * h;
        assert(pixel_num > 0);

        offscreen_viewport.background = Color(1.0f, 1.0f, 1.0f, 1.0f);
        offscreen_viewport.material_override->set_int(RasterizerOptions::DepthTest, GL_TRUE);
        offscreen_viewport.material_override->set_int(RasterizerOptions::CullFaceEnabled, GL_TRUE);
        offscreen_viewport.material_override->set_int(RasterizerOptions::ColorMask, GL_TRUE);

        // Render object IDs
        render_viewport(r, get_objectid_viewport_entity(r));

        auto& buffer = read_pixels(r, offscreen_viewport, x, y, w, h);

        // Convert colors to ids
        const size_t pixel_size = 4;
        for (auto i = 0; i < pixel_num; i++) {
            auto id = color_to_id(
                buffer[pixel_size * i + 0],
                buffer[pixel_size * i + 1],
                buffer[pixel_size * i + 2]);

            if (!is_id_background(id) && r.valid(Entity(id))) {
                r.emplace_or_replace<HoveredTemp>(Entity(id));
            }
        }


    } else {
        const auto& frustum = sel_ctx.frustum;

        offscreen_viewport.material_override->set_int(RasterizerOptions::Query, GL_SAMPLES_PASSED);
        offscreen_viewport.material_override->set_int(RasterizerOptions::DepthTest, GL_FALSE);
        offscreen_viewport.material_override->set_int(RasterizerOptions::CullFaceEnabled, GL_FALSE);
        offscreen_viewport.material_override->set_int(RasterizerOptions::ColorMask, GL_FALSE);

        int num_queries = 0;

        // Add GLQuery component to all bounded entities
        for (auto e : r.view<Bounds, MeshRender>()) {
            // Layer visibility test
            if (!ui::is_visible_in(
                    r,
                    e,
                    offscreen_viewport.visible_layers,
                    offscreen_viewport.hidden_layers)) {
                continue;
            }

            if (!r.get<MeshRender>(e).material) continue;

            const auto& bb = r.get<Bounds>(e).global;

            // Check if frustum completely contains the AABB (in case the object is too small to
            // produce render fragments)
            bool all_in = false;
            const bool isect = frustum.intersects(bb, all_in);

            if (all_in) {
                r.emplace<HoveredTemp>(e);
            } else if (isect) {
                GLuint id;
                glGenQueries(1, &id); // TODO cache these in a pool somewhere
                r.emplace<GLQuery>(e, GLQuery{GL_SAMPLES_PASSED, id, 0});
                num_queries++;
            }
        }

        if (num_queries > 0) {
            // Render with occlusion query and no color output
            render_viewport(r, get_objectid_viewport_entity(r));

            // Check occlusion query results
            auto v = r.view<GLQuery>();
            for (auto e : v) {
                auto& q = v.get<GLQuery>(e);

                GLint result = q.result;
                if (result > 0) {
                    r.emplace<HoveredTemp>(e);
                }
                glDeleteQueries(1, &q.id);
                r.remove<GLQuery>(e);
            }
        }
    }
}


/// Updates <Hovered> component based on current selection context
void update_mesh_hovered(Registry& r)
{
    // Copy selection context
    SelectionContext sel_ctx = get_selection_context(r);

    if (!r.valid(sel_ctx.active_viewport)) return;
    if (sel_ctx.element_type != entt::resolve<ElementObject>().id()) return;

    if (!sel_ctx.marquee_active) {
        sel_ctx.marquee_active = true;
        sel_ctx.frustum = sel_ctx.onepx_frustum;
        sel_ctx.select_backfacing = false;
        sel_ctx.viewport_min = sel_ctx.viewport_position;
        sel_ctx.viewport_max = sel_ctx.viewport_min + Eigen::Vector2i::Ones();
    }
    update_mesh_hovered_GL(r, sel_ctx);

    auto not_hovered_anymore = r.view<Hovered>(entt::exclude<HoveredTemp>);
    r.remove<Hovered>(not_hovered_anymore.begin(), not_hovered_anymore.end());

    auto v = r.view<HoveredTemp>();
    for (auto e : v) {
        ui::set_hovered(r, e, SelectionBehavior::ADD);
    }
    r.remove<HoveredTemp>(v.begin(), v.end());
}


} // namespace ui
} // namespace lagrange
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

#include <lagrange/ui/utils/viewport.h>
#include <lagrange/ui/default_shaders.h>
#include <lagrange/ui/panels/ViewportPanel.h>
#include <lagrange/ui/types/Material.h>
#include <lagrange/ui/components/ObjectIDViewport.h>
#include <lagrange/ui/components/Viewport.h>
#include <lagrange/ui/components/SelectionContext.h>

namespace lagrange {
namespace ui {

namespace {

struct GLColorTempBuffer
{
    std::vector<unsigned char> buffer;
};
}

int color_to_id(unsigned char r, unsigned char g, unsigned char b)
{
    return int(r) + (int(g) << 8) + (int(b) << 16);
}


bool is_id_background(int id)
{
    return id == 16777215;
}
const std::vector<unsigned char>&
read_pixels(Registry& r, ViewportComponent& v, int x, int y, int w, int h)
{
    // Allocate temporary buffer
    const size_t pixel_size = 4;
    auto& screen_data = r.ctx_or_set<GLColorTempBuffer>().buffer;
    screen_data.resize(pixel_size * w * h);


    GLScope gl;

    v.fbo->bind();
    // Read pixels from texture
    gl(glPixelStorei, GL_UNPACK_ALIGNMENT, 1);
    gl(glReadPixels, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, screen_data.data());

    return screen_data;
}


ViewportComponent& setup_offscreen_viewport(
    Registry& r,
    Entity /*ofscreen_viewport_entity*/,
    Entity active_viewport_entity,
    StringID override_shader)
{
    // Get object id viewport
    auto& offscreen_viewport = r.get<ViewportComponent>(get_objectid_viewport_entity(r));

    offscreen_viewport.material_override = std::make_shared<Material>(r, override_shader);

    // Reset rasterizer options
    // offscreen_viewport.material_override->int_values.clear();

    // Make sure it matches the size of the currently active selection viewport
    auto& active_selection_viewport = r.get<ViewportComponent>(active_viewport_entity);
    offscreen_viewport.width = active_selection_viewport.width;
    offscreen_viewport.height = active_selection_viewport.height;
    offscreen_viewport.visible_layers = active_selection_viewport.visible_layers;
    offscreen_viewport.hidden_layers = active_selection_viewport.hidden_layers;

    return offscreen_viewport;
}


std::tuple<int, int, int, int>
setup_scissor(Registry& /*r*/, ViewportComponent& offscreen_viewport, const SelectionContext &sel_ctx)
{
    // Calculate in-viewport coordinates
    const int x = int(sel_ctx.viewport_min.x());
    const int y = int(sel_ctx.viewport_min.y());
    const int w = int(sel_ctx.viewport_max.x() - sel_ctx.viewport_min.x());
    const int h = int(sel_ctx.viewport_max.y() - sel_ctx.viewport_min.y());
    {
        offscreen_viewport.material_override->set_int(RasterizerOptions::ScissorTest, GL_TRUE);
        offscreen_viewport.material_override->set_int(RasterizerOptions::ScissorX, x);
        offscreen_viewport.material_override->set_int(RasterizerOptions::ScissorY, y);
        offscreen_viewport.material_override->set_int(RasterizerOptions::ScissorWidth, w);
        offscreen_viewport.material_override->set_int(RasterizerOptions::ScissortHeight, h);
    }

    return {x, y, w, h};
}

} // namespace ui
} // namespace lagrange

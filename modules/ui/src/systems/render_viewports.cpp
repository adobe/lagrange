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

#include <lagrange/ui/components/RenderContext.h>
#include <lagrange/ui/components/ShadowMap.h>
#include <lagrange/ui/systems/render_background.h>
#include <lagrange/ui/systems/render_geometry.h>
#include <lagrange/ui/systems/render_shadowmaps.h>
#include <lagrange/ui/systems/render_viewports.h>
#include <lagrange/ui/systems/update_scene_bounds.h>
#include <lagrange/ui/types/Camera.h>
#include <lagrange/ui/utils/bounds.h>
#include <lagrange/ui/utils/render.h>
#include <lagrange/ui/utils/viewport.h>


namespace lagrange {
namespace ui {


void adjust_camera(Registry& registry, ViewportComponent& viewport, Camera& cam)
{
    const AABB bb = get_scene_bounds(registry).global;

    // Skip for empty scene
    if (bb.isEmpty()) return;

    float nearest = get_nearest_bounds_distance(
        registry,
        cam.get_position(),
        viewport.visible_layers,
        viewport.hidden_layers);
    float furthest = get_furthest_bounds_distance(
        registry,
        cam.get_position(),
        viewport.visible_layers,
        viewport.hidden_layers);

    auto near_plane = cam.get_near();
    auto far_plane = cam.get_far();

    if (furthest > 0) {
        far_plane = 1.01f * furthest;
    }
    if (nearest > 0) {
        near_plane = 0.99f * nearest;
    }
    cam.set_planes(near_plane, far_plane);
}


void render_viewport(Registry& r, Entity e)
{
    auto& viewport = r.get<ViewportComponent>(e);

    // Update camera
    {
        auto& cam = get_viewport_camera(r, viewport);
        viewport.computed_camera = cam;
        viewport.computed_camera.set_window_dimensions(
            float(viewport.width),
            float(viewport.height));

        // Adjust camera if needed
        if (viewport.auto_nearfar) {
            adjust_camera(r, viewport, viewport.computed_camera);
        }
    }


    auto& fbo_ptr = viewport.fbo;

    // Set this viewport as current in the render context
    auto& rctx = get_render_context(r);
    rctx.viewport = e;
    rctx.polygon_offset_layer = 0;

    GLScope gl;
    utils::render::set_render_pass_defaults(gl);
    if (fbo_ptr) {
        fbo_ptr->bind();
        fbo_ptr->resize_attachments(
            int(viewport.computed_camera.get_window_width()),
            int(viewport.computed_camera.get_window_height()));

        if (fbo_ptr->get_color_attachement(0) && fbo_ptr->is_srgb()) {
#if defined(__EMSCRIPTEN__)
            // TODO WebGL: GL_FRAMEBUFFER_SRGB is not supported.
            // See https://developer.mozilla.org/en-US/docs/Web/API/EXT_sRGB and
            // https://developer.mozilla.org/en-US/docs/Web/API/WebGLRenderingContext/getFramebufferAttachmentParameter.
            // logger().warn("GL_FRAMEBUFFER_SRGB is not supported in WebGL.");
#else
            gl(glEnable, GL_FRAMEBUFFER_SRGB);
#endif
        }

        gl(glViewport,
           0,
           0,
           int(viewport.computed_camera.get_window_width()),
           int(viewport.computed_camera.get_window_height()));


        if (viewport.clear_bits != 0) {
            gl(glClearColor,
               viewport.background.x(),
               viewport.background.y(),
               viewport.background.z(),
               viewport.background.w());

            gl(glClear, viewport.clear_bits);
        }
    }

    // TODO as geometry (skybox)!
    render_background(r);

    render_geometry(r);

    render_post_process(r);
}

void render_viewports(Registry& registry)
{
    // Sort viewports, ShadowMaps should always precede other viewport
    registry.sort<ViewportComponent>([&](Entity a, Entity b) {
        const bool is_shadowmap_a = registry.all_of<ShadowMap>(a);
        const bool is_shadowmap_b = registry.all_of<ShadowMap>(b);
        return is_shadowmap_a > is_shadowmap_b;
    });

    auto view = registry.view<ViewportComponent>();

    // Render
    for (auto e : view) {
        auto& viewport = view.get<ViewportComponent>(e);
        if (viewport.enabled) {
            render_viewport(registry, e);
        }
    }
}


} // namespace ui
} // namespace lagrange

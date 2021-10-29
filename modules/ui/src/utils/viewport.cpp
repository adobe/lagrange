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
#include <lagrange/ui/panels/ViewportPanel.h>
#include <lagrange/ui/types/Camera.h>

#include <lagrange/ui/components/ObjectIDViewport.h>
#include <lagrange/ui/components/SelectionViewport.h>
#include <lagrange/ui/default_entities.h>
#include <lagrange/ui/default_shaders.h>

// is this needed? use utils instead
#include <lagrange/ui/components/CameraComponents.h>


namespace lagrange {
namespace ui {


void instance_camera_to_viewports(Registry& registry, Entity source_viewport)
{
    const auto camera_entity = registry.get<ViewportComponent>(source_viewport).camera_reference;

    for (auto e : registry.view<ViewportComponent>()) {
        registry.get<ViewportComponent>(e).camera_reference = camera_entity;
    }
}

void copy_camera_to_viewports(Registry& registry, Entity source_viewport)
{
    const auto camera_entity = registry.get<ViewportComponent>(source_viewport).camera_reference;

    for (auto e : registry.view<ViewportComponent>()) {
        if (e == source_viewport) continue;
        auto& viewport = registry.get<ViewportComponent>(e);
        auto new_camera = registry.create();

        registry.emplace<Camera>(new_camera, registry.get<Camera>(camera_entity));
        registry.emplace<CameraController>(
            new_camera,
            registry.get<CameraController>(camera_entity));

        if (registry.has<CameraTurntable>(camera_entity)) {
            registry.emplace<CameraTurntable>(
                new_camera,
                registry.get<CameraTurntable>(camera_entity));
        }

        viewport.camera_reference = new_camera;
    }
}

ViewportPanel& get_focused_viewport_panel(Registry& registry)
{
    return registry.get<ViewportPanel>(registry.ctx<FocusedViewportPanel>().viewport_panel);
}

Entity get_focused_viewport_entity(Registry& registry)
{
    return get_focused_viewport_panel(registry).viewport;
}

ViewportComponent& get_focused_viewport(Registry& registry)
{
    return registry.get<ViewportComponent>(get_focused_viewport_entity(registry));
}

Entity get_focused_camera_entity(Registry& registry)
{
    return get_focused_viewport(registry).camera_reference;
}


Entity get_hovered_viewport_panel_entity(Registry& registry)
{
    auto view = registry.view<const ViewportPanel>();
    for (auto e : view) {
        if (view.get<const ViewportPanel>(e).hovered) {
            return e;
        }
    }
    return NullEntity;
}

Entity get_hovered_viewport_entity(Registry& registry)
{
    auto window = get_hovered_viewport_panel_entity(registry);
    if (registry.valid(window)) {
        return registry.get<ViewportPanel>(window).viewport;
    }
    return NullEntity;
}

Camera& get_camera(Registry& registry, Entity e)
{
    return registry.get<Camera>(e);
}

Camera& get_focused_camera(Registry& registry)
{
    return get_camera(registry, get_focused_camera_entity(registry));
}


void camera_focus_and_fit(
    Registry& registry,
    Entity camera,
    bool focus,
    bool fit,
    float duration_seconds /*= 1.0f*/,
    const std::function<bool(Registry& r, Entity e)>& filter /*= nullptr */)
{
    
    auto& c = registry.emplace_or_replace<CameraFocusAndFit>(camera, CameraFocusAndFit{});
    c.filter = filter;
    c.focus = focus;
    c.fit = fit;
    c.speed =
        duration_seconds > 0.0f ? 1.0f / duration_seconds : std::numeric_limits<float>::infinity();
}

Entity get_selection_viewport_entity(const Registry& registry)
{
    return registry.ctx<SelectionViewport>().viewport_entity;
}

Entity get_objectid_viewport_entity(const Registry& registry)
{
    return registry.ctx<ObjectIDViewport>().viewport_entity;
}

void camera_zoom_to_fit(
    Registry& /*registry*/,
    Entity /*camera*/,
    float /*duration_seconds*/,
    const std::function<bool(Registry& r, Entity e)>& /*filter*/ /*= nullptr*/)
{}

void camera_focus_on(
    Registry& registry,
    Entity camera,
    float duration_seconds,
    const std::function<bool(Registry& r, Entity e)>& filter /*= nullptr*/)
{
    CameraFocusAndFit c;
    c.current_time = 0;
    c.filter = filter;
    c.speed =
        duration_seconds > 0.0f ? 1.0f / duration_seconds : std::numeric_limits<float>::infinity();

    registry.emplace_or_replace<CameraFocusAndFit>(camera, c);
}


Entity add_viewport(Registry& registry, Entity camera_entity, bool srgb /*= false*/)
{
    auto e = registry.create();

    ViewportComponent viewport;

    viewport.camera_reference = camera_entity;

    auto& camera = get_viewport_camera(registry, viewport);

    viewport.width = int(camera.get_window_width());
    viewport.height = int(camera.get_window_height());

    // Create framebuffer resource

    auto cparam = Texture::Params::rgba16f();
    if (srgb) {
        cparam.internal_format = GL_SRGB_ALPHA;
        cparam.sRGB = true;
    }

    auto color_tex = std::make_shared<Texture>(cparam);
    auto depth_tex = std::make_shared<Texture>(Texture::Params::depth());

    auto fbo = std::make_shared<FrameBuffer>();
    fbo->set_color_attachement(0, color_tex);
    fbo->set_depth_attachement(depth_tex);
    viewport.fbo = fbo;

    registry.emplace<ViewportComponent>(e, std::move(viewport));

    return e;
}

void add_selection_outline_post_process(Registry& registry, Entity viewport_entity)
{
    auto& vc = registry.get<ViewportComponent>(viewport_entity);

    auto sel_viewport = get_selection_viewport_entity(registry);

    auto color = registry.get<ViewportComponent>(sel_viewport).fbo->get_color_attachement(0);
    auto depth = registry.get<ViewportComponent>(sel_viewport).fbo->get_depth_attachment();

    {
        auto mat = create_material(registry, DefaultShaders::Outline);
        mat->set_texture("color_tex", color);
        mat->set_texture("depth_tex", depth);
        const Color dark_orange = Color(252.0f / 255.0f, 86.0f / 255.0f, 3.0f / 255.0f, 1.0f);
        mat->set_color("in_color", dark_orange);
        mat->set_int(RasterizerOptions::DepthTest, GL_FALSE);
        mat->set_int(RasterizerOptions::BlendEquation, GL_FUNC_ADD);
        mat->set_int(RasterizerOptions::BlendSrcRGB, GL_SRC_ALPHA);
        mat->set_int(RasterizerOptions::BlendDstRGB, GL_ONE_MINUS_SRC_ALPHA);
        mat->set_int(RasterizerOptions::BlendSrcAlpha, GL_ONE);
        mat->set_int(RasterizerOptions::BlendDstAlpha, GL_ONE);
        vc.post_process_effects["SelectionOutline"] = mat;
    }
}


} // namespace ui
} // namespace lagrange

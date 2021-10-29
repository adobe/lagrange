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

#include <lagrange/ui/types/Camera.h>
#include <lagrange/ui/types/Color.h>
#include <lagrange/ui/Entity.h>
#include <lagrange/ui/types/FrameBuffer.h>
#include <lagrange/ui/components/Layer.h>
#include <unordered_map>

namespace lagrange {
namespace ui {

class RenderPassBase;
class Renderer;
class Material;

// On or offscreen viewport
struct ViewportComponent
{
    Entity camera_reference = NullEntity;
    Camera computed_camera;


    bool enabled = true;

    int width = 1;
    int height = 1;
    bool auto_nearfar = true;

    std::set<RenderPassBase*> selected_passes;
    std::shared_ptr<FrameBuffer> fbo;


    GLenum query_type;

    Layer visible_layers = Layer(true);
    Layer hidden_layers = Layer(false);
    std::shared_ptr<Material> material_override;


    std::map<std::string, std::shared_ptr<Material>> post_process_effects;

    Color background = Color(0, 0, 0, 0);
    GLbitfield clear_bits = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
};

inline Camera& get_viewport_camera(Registry& r, ViewportComponent& viewport)
{
    if (r.valid(viewport.camera_reference)) {
        return r.get<Camera>(viewport.camera_reference);
    } else {
        return viewport.computed_camera;
    }
}

inline const Camera& get_viewport_camera(const Registry& r, const ViewportComponent& viewport)
{
    if (r.valid(viewport.camera_reference)) {
        return r.get<Camera>(viewport.camera_reference);
    } else {
        return viewport.computed_camera;
    }
}

inline Camera& get_viewport_camera(Registry& r, Entity e)
{
    assert(r.has<ViewportComponent>(e));
    return get_viewport_camera(r, r.get<ViewportComponent>(e));
}

inline const Camera& get_viewport_camera(const Registry& r, Entity e)
{
    assert(r.has<ViewportComponent>(e));
    return get_viewport_camera(r, r.get<ViewportComponent>(e));
}

} // namespace ui
} // namespace lagrange
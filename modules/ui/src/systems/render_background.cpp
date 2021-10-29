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

#include <lagrange/ui/systems/render_background.h>

#include <lagrange/ui/components/Common.h>
#include <lagrange/ui/components/IBL.h>
#include <lagrange/ui/components/RenderContext.h>
#include <lagrange/ui/default_shaders.h>
#include <lagrange/ui/types/ShaderLoader.h>
#include <lagrange/ui/utils/layer.h>
#include <lagrange/ui/utils/render.h>


namespace lagrange {
namespace ui {

struct SkyboxCubeVertexData
{
    std::shared_ptr<VertexData> vertex_data;
};

void render_background(Registry& r)
{
    auto& viewport = get_render_context_viewport(r);

    // Grab and bind fbo if set
    auto& fbo = viewport.fbo;
    if (fbo) {
        fbo->bind();
    }

    r.ctx_or_set<entt::resource_cache<Shader>>();
    const auto& camera = viewport.computed_camera;

    auto shader_res = get_shader(r, DefaultShaders::Skybox);
    if (!shader_res) return;
    auto shader = *shader_res;

    auto& skybox_data = r.ctx_or_set<SkyboxCubeVertexData>();
    if (!skybox_data.vertex_data) skybox_data.vertex_data = generate_cube_vertex_data();


    shader.bind();
    GLScope gl;


    gl(glFrontFace, GL_CW);


    r.view<IBL>().each([&](const Entity& entity, IBL& ibl) {
        if (!ui::is_visible_in(r, entity, viewport.visible_layers, viewport.hidden_layers)) return;

        if (!ibl.show_skybox) return;

        // Do not show in selection
        if (viewport.visible_layers.test(DefaultLayers::Selection)) return;

        // View matrix without translation
        Eigen::Matrix4f V = Eigen::Matrix4f::Identity();
        V.block<3, 3>(0, 0) = camera.get_view().block<3, 3>(0, 0);

        // If camera is orthographic, force perspective for environment map
        if (camera.get_type() == Camera::Type::ORTHOGRAPHIC) {
            auto pcam = camera;
            pcam.set_type(Camera::Type::PERSPECTIVE);
            shader["PV"] = (pcam.get_perspective() * V).eval();
        } else {
            shader["PV"] = (camera.get_perspective() * V).eval();
        }

        shader["mip_level"] = ibl.blur;
        ibl.background->bind_to(GL_TEXTURE0 + 0);

        render_vertex_data(*skybox_data.vertex_data, GL_TRIANGLES, 3);
    });

    gl(glFrontFace, GL_CCW);
}


} // namespace ui
} // namespace lagrange

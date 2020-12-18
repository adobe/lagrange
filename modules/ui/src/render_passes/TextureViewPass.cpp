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
#include <lagrange/ui/MeshBufferFactory.h>
#include <lagrange/ui/RenderPipeline.h>
#include <lagrange/ui/RenderResourceBuilder.h>
#include <lagrange/ui/render_passes/TextureViewPass.h>

namespace lagrange {
namespace ui {

template <>
std::string default_render_pass_name<TextureViewPass>()
{
    return "TextureViewPass";
}


std::unique_ptr<TextureViewPass> create_textureview_pass(
    Resource<CommonPassData> common,
    RenderPipeline& pipeline)
{

    RenderPipeline* pipeline_ptr = &pipeline;

    auto pass = std::make_unique<TextureViewPass>(
        default_render_pass_name<TextureViewPass>(),
        [=](TextureViewPassData& data, OptionSet& opt, RenderResourceBuilder& builder) {
            data.shader = builder.create<Shader>("texture",
                ShaderResourceParams{ShaderResourceParams::Tag::VIRTUAL_PATH, "texture.shader"});

            data.common = common;

            data.pipeline = pipeline_ptr;

            opt.add<int>("Texture ID", -1);
            opt.add<bool>("Normalize", false);
            opt.add<float>("NormalizeMin", 0.1f);
            opt.add<float>("NormalizeMax", 10.0f);
            opt.add<float>("Bias", 1.0f);
            opt.add<bool>("Linearize Depth", true);
            opt.add<int>("Cubemap face", 0, 0, 5);
        },
        [](const TextureViewPassData& data, const OptionSet& opt) {
            auto& shader = *data.shader;
            auto& fbo = *data.common->final_output_fbo;
            auto& quad = MeshBuffer::quad(); 
            const auto& camera = *data.common->camera;


            const auto tex_id = opt.get<int>("Texture ID");
            if (tex_id < 0) return;

            auto tex_resources = data.pipeline->get_resources<Texture>();
            auto tex_it =
                std::find_if(tex_resources.begin(), tex_resources.end(), [=](const Resource<Texture> & t) {
                    return t.has_value() && int(t->get_id()) == tex_id;
                });

            if (tex_it == tex_resources.end()) return;

            auto& tex = (**tex_it);
            const auto& p = tex.get_params();

            GLScope gl;
            fbo.bind();

            if (fbo.is_srgb()) {
                gl(glEnable, GL_FRAMEBUFFER_SRGB);
            }
            else {
                gl(glDisable, GL_FRAMEBUFFER_SRGB);
            }

            shader.bind();

            gl(glViewport, 0, 0, int(camera.get_window_width()), int(camera.get_window_height()));


            GL(glActiveTexture(GL_TEXTURE0));
            GL(glBindTexture(GL_TEXTURE_CUBE_MAP, 0));
            GL(glBindTexture(GL_TEXTURE_2D, 0));

            GL(glActiveTexture(GL_TEXTURE1));
            GL(glBindTexture(GL_TEXTURE_CUBE_MAP, 0));
            GL(glBindTexture(GL_TEXTURE_2D, 0));

            if (tex.get_params().type == GL_TEXTURE_CUBE_MAP) {
                tex.bind_to(GL_TEXTURE0 + 1);
                shader["is_cubemap"] = true;
                shader["cubemap_face"] = opt.get<int>("Cubemap face");
            }
            else {
                tex.bind_to(GL_TEXTURE0 + 0);
                shader["is_cubemap"] = false;
            }

            shader["tex"] = 0;
            shader["tex_cube"] = 1;

            if ((p.format == GL_DEPTH_COMPONENT || p.internal_format == GL_DEPTH_COMPONENT24)) {
                if (opt.get<bool>("Linearize Depth")) {
                    shader["is_depth"] = true;
                    shader["depth_near"] = camera.get_near();
                    shader["depth_far"] = camera.get_far();
                }
                else {
                    shader["is_depth"] = false;
                    shader["singleChannel"] = true;
                }
            }
            else {
                shader["is_depth"] = false;
                shader["singleChannel"] = false;
            }

            shader["bias"] = opt.get<float>("Bias");

            if (opt.get<bool>("Normalize")) {
                shader["normalize_values"] = true;
                shader["normalize_min"] = opt.get<float>("NormalizeMin");
                shader["normalize_range"] =
                    opt.get<float>("NormalizeMax") - opt.get<float>("NormalizeMin");
            }
            else {
                shader["normalize"] = false;
            }

            quad.render(MeshBuffer::Primitive::TRIANGLES);
        });

    pass->add_tag("default");
    pass->add_tag("texture");
    pass->add_tag("post");

    return pass;
}

} // namespace ui
} // namespace lagrange

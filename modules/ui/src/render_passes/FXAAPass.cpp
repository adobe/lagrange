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
#include <lagrange/ui/render_passes/FXAAPass.h>
#include <lagrange/ui/RenderResourceBuilder.h>
#include <lagrange/ui/MeshBufferFactory.h>

namespace lagrange {
namespace ui {

template <>
std::string default_render_pass_name<FXAAPass>()
{
    return "FXAAPass";
}


std::unique_ptr<FXAAPass> create_fxaa_pass(Resource<CommonPassData> common)
{
    auto pass = std::make_unique<FXAAPass>(
        default_render_pass_name<FXAAPass>(),
        [=](FXAAPassData& data, OptionSet&, RenderResourceBuilder& builder) {
            data.shader = builder.create<Shader>(
                "FXAA",
                ShaderResourceParams{ShaderResourceParams::Tag::VIRTUAL_PATH, "post/FXAA.shader"});

            data.texture_shader = builder.create<Shader>("texture",
                ShaderResourceParams{ShaderResourceParams::Tag::VIRTUAL_PATH, "texture.shader"});

            Texture::Params p_color = Texture::Params::rgba16f();
            p_color.internal_format = GL_SRGB_ALPHA;

            
            FBOResourceParams fbo_params;
            fbo_params.color_attachment_0 = builder.create<Texture>("fxaa_tex", p_color);

            data.target_fbo = builder.create<FrameBuffer>("fxaa_fbo", fbo_params);
            data.common = common;
        },
        [](const FXAAPassData& data, const OptionSet&) {
            auto& shader = *data.shader;
            auto& texture_shader = *data.texture_shader;
            auto& quad = MeshBuffer::quad();
            auto& fbo = *data.target_fbo;
            auto& source_fbo = *data.common->final_output_fbo;
            auto& camera = *data.common->camera;

            auto source_color = source_fbo.get_color_attachement(0);
            if (!source_color) return;


            const auto w = int(camera.get_window_width());
            const auto h = int(camera.get_window_height());

            /*
                Grab final fbo and filter it to temporary fbo
            */
            {
                GLScope gl;
                fbo.bind();
                if (fbo.is_srgb()) {
                    gl(glEnable, GL_FRAMEBUFFER_SRGB);
                }
                else {
                    gl(glDisable, GL_FRAMEBUFFER_SRGB);
                }

                fbo.resize_attachments(w, h);
                gl(glViewport, 0, 0, w, h);

                // Prep shader
                shader.bind();
                shader["RCPFrame"] = Eigen::Vector2f(1.0f / float(w), 1.0f / float(h));

                // Bind texture
                source_color->bind_to(GL_TEXTURE0 + 0);
                shader["uSourceTex"] = 0;
                quad.render(MeshBuffer::Primitive::TRIANGLES);
            }
            // Render back to final fbo
            {
                GLScope gl;
                source_fbo.bind();
                if (source_fbo.is_srgb()) {
                    gl(glEnable, GL_FRAMEBUFFER_SRGB);
                }
                else {
                    gl(glDisable, GL_FRAMEBUFFER_SRGB);
                }

                source_fbo.resize_attachments(w, h);
                gl(glViewport, 0, 0, w, h);

                texture_shader.bind();
                fbo.get_color_attachement(0)->bind_to(GL_TEXTURE0 + 0);
                texture_shader["tex"] = 0;
                texture_shader["tex_cube"] = 1;
                texture_shader["is_depth"] = false;
                texture_shader["is_cubemap"] = false;
                texture_shader["normalize"] = false;

                quad.render(MeshBuffer::Primitive::TRIANGLES);
            }
        });

    pass->add_tag("default");
    pass->add_tag("fxaa");
    pass->add_tag("post");

    return pass;
}

} // namespace ui
} // namespace lagrange

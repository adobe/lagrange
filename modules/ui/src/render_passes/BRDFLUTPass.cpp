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
#include <lagrange/ui/Light.h>
#include <lagrange/ui/MeshBufferFactory.h>
#include <lagrange/ui/RenderResourceBuilder.h>
#include <lagrange/ui/Scene.h>
#include <lagrange/ui/render_passes/BRDFLUTPass.h>

namespace lagrange {
namespace ui {


template <>
std::string default_render_pass_name<BRDFLUTPass>()
{
    return "BRDFLUTPass";
}


std::unique_ptr<BRDFLUTPass> create_BRDFLUT_Pass(Resource<CommonPassData> common)
{
    auto brdf_lut_pass = std::make_unique<BRDFLUTPass>(
        default_render_pass_name<BRDFLUTPass>(),
        [=](BRDFLUTPassData& data, OptionSet& LGUI_UNUSED(options), RenderResourceBuilder& builder) {
            data.brdf_lut_output = builder.create<Texture>(
                "_brdf_lut", Texture::Params::rgb16f());
            data.temp_fbo =
                builder.create<FrameBuffer>("_brdf_lut_fbo", FBOResourceParams{data.brdf_lut_output});
            data.shader = builder.create<Shader>("_brdf_lut_shader",
                ShaderResourceParams{
                    ShaderResourceParams::Tag::VIRTUAL_PATH,
                    "util/brdf_lut.shader"});
            data.common = common;
        },
        [](const BRDFLUTPassData& data, const OptionSet& LGUI_UNUSED(options)) {
            auto& shader = *data.shader;
            auto& fbo = *data.temp_fbo;

            fbo.bind();
            fbo.resize_attachments(512, 512);

            GLScope gl;
            gl(glDisable, GL_MULTISAMPLE);
            gl(glDisable, GL_DEPTH_TEST);
            gl(glDisable, GL_BLEND);
            gl(glDisable, GL_CULL_FACE);

            gl(glViewport, 0, 0, 512, 512);
            gl(glClearColor, 0.0f, 0.0f, 0.0f, 0.0f);
            gl(glClear, GL_COLOR_BUFFER_BIT);

            shader.bind();
            MeshBuffer::quad().render(MeshBuffer::Primitive::TRIANGLES);
            shader.unbind();
        });

    // After first pass, resources get deleted and pass is not executed
    brdf_lut_pass->set_one_shot(true);
    brdf_lut_pass->add_tag("default");
    brdf_lut_pass->add_tag("pbr");

    return brdf_lut_pass;
}

} // namespace ui
} // namespace lagrange

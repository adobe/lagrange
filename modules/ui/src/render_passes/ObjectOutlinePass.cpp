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
#include <lagrange/ui/RenderResourceBuilder.h>
#include <lagrange/ui/Scene.h>
#include <lagrange/ui/render_passes/ObjectOutlinePass.h>


#include <lagrange/ui/MeshBufferFactory.h>



namespace lagrange {
namespace ui {

template <>
std::string default_render_pass_name<ObjectOutlinePass>()
{
    return "ObjectOutlinePass";
}


std::unique_ptr<ObjectOutlinePass> create_object_outline_pass(
    const RenderPass<Viz::PassData>* object_id_viz,
    Resource<CommonPassData> common)
{
    auto pass = std::make_unique<ObjectOutlinePass>(
        default_render_pass_name<ObjectOutlinePass>(),
        [=](ObjectOutlinePassData& data, OptionSet& options, RenderResourceBuilder& builder) {
            data.input_color = builder.read(object_id_viz->get_data().color_buffer);
            data.input_depth = builder.read(object_id_viz->get_data().depth_buffer);

            data.shader = builder.create<Shader>("post/outline.shader",
                ShaderResourceParams{
                    ShaderResourceParams::Tag::VIRTUAL_PATH,
                    "post/outline.shader"});

            options.add<float>("Backface alpha", 0.15f, 0.0f, 0.0f);
            options.add<Color>("Color", Color(1.0f, 0.0f, 0.0f));

            data.common = common;
        },
        [](const ObjectOutlinePassData& data, const OptionSet& options) {
            const auto& camera = *data.common->camera;
            auto& quad = MeshBuffer::quad();

            auto& shader = *data.shader;
            
            const auto& input_color = *data.input_color;
            const auto& input_depth = *data.input_depth;

            auto& pass_counter = *data.common->pass_counter;
            pass_counter += 1;


            // for now
            auto& fbo = *data.common->final_output_fbo;
            GLScope gl;
            fbo.bind();
            shader.bind();

            gl(glEnable, GL_BLEND);
            gl(glBlendFuncSeparate, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
            gl(glBlendEquation, GL_FUNC_ADD);
            gl(glDisable, GL_MULTISAMPLE);
            
            gl(glViewport, 0, 0, int(camera.get_window_width()), int(camera.get_window_height()));


            
            shader["uniform_color"] = options.get<Color>("Color").to_vec4();
            shader["depth_offset"] = (*data.common->pass_counter) * -0.000001f;

            input_color.bind_to(GL_TEXTURE0 + 0);
            shader["color_tex"] = 0;

            input_depth.bind_to(GL_TEXTURE0 + 1);
            shader["depth_tex"] = 1;

            gl(glEnable, GL_DEPTH_CLAMP);
            gl(glDepthFunc, GL_LEQUAL);
            gl(glEnable, GL_POLYGON_OFFSET_FILL);
            gl(glPolygonOffset, 1.0f, pass_counter * -1.0f);

            
            gl(glDepthMask, GL_FALSE);
            
            /////
            gl(glDisable, GL_DEPTH_TEST);
            /////
            shader["alpha_multiplier"] = options.get<float>("Backface alpha");
            quad.render(MeshBuffer::Primitive::TRIANGLES);

            /////
            gl(glEnable, GL_DEPTH_TEST);
            /////
            shader["alpha_multiplier"] = 1.0f;
            quad.render(MeshBuffer::Primitive::TRIANGLES);
           
        });

    pass->add_tag("default");
    pass->add_tag("outline");
    pass->add_tag("post");

    return pass;
}

} // namespace ui
} // namespace lagrange

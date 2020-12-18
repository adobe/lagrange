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
#include <lagrange/ui/MeshBuffer.h>
#include <lagrange/ui/MeshBufferFactory.h>
#include <lagrange/ui/RenderResourceBuilder.h>
#include <lagrange/ui/render_passes/ImmediatePass.h>
#include <lagrange/ui/RenderUtils.h>

namespace lagrange {
namespace ui {

template <>
std::string default_render_pass_name<ImmediatePass>()
{
    return "ImmediatePass";
}


std::unique_ptr<ImmediatePass> create_immediate_pass(
    Resource<CommonPassData> common,
    Resource<ImmediateData> immediate_data)
{
    auto pass = std::make_unique<ImmediatePass>(
        default_render_pass_name<ImmediatePass>(),
        [=](ImmediatePassData& data, OptionSet& opt, RenderResourceBuilder& builder) {
            data.common = common;

            data.buffer = builder.create<MeshBuffer>("immediate_data_buffer");
            data.immediate_data = immediate_data;
            data.shader = builder.create<Shader>("surface/simple.shader",
                ShaderResourceParams{
                    ShaderResourceParams::Tag::VIRTUAL_PATH,
                    "surface/simple.shader"});

            opt.add<float>("Point Size", 4.0f, 1.0f);
            opt.add<bool>("Depth Test", true);
        },
        [](const ImmediatePassData& data, const OptionSet& opt) {
            auto& fbo = *data.common->final_output_fbo;
            auto& shader = *data.shader;
            auto& camera = *data.common->camera;
            auto& local_immediate_data = *data.immediate_data;

            auto& pass_counter = *data.common->pass_counter;
            pass_counter += 1;

            auto& buffer = *data.buffer;
            
            auto move_vec = [](std::vector<Eigen::Vector3f>& vec_data, VertexBuffer& vbo) {
                VertexBuffer::DataDescription dd;
                dd.gl_type = GL_FLOAT;
                dd.integral = false;
                dd.count = GLsizei(vec_data.size());
                vbo.upload(vec_data.data(), sizeof(float) * 3 * dd.count, dd);
            };

            auto move_color = [](std::vector<Color>& color_data, VertexBuffer& vbo) {
                VertexBuffer::DataDescription dd;
                dd.gl_type = GL_FLOAT;
                dd.integral = false;
                dd.count = GLsizei(color_data.size());
                vbo.upload(color_data.data(), sizeof(float) * 4 * dd.count, dd);
            };


            move_vec(local_immediate_data.points,
                buffer.get_sub_buffer(MeshBuffer::SubBufferType::POSITION, "points"));
            move_color(local_immediate_data.points_colors,
                buffer.get_sub_buffer(MeshBuffer::SubBufferType::COLOR, "points"));

            move_vec(local_immediate_data.triangles,
                buffer.get_sub_buffer(MeshBuffer::SubBufferType::POSITION, "triangles"));
            move_color(local_immediate_data.triangles_colors,
                buffer.get_sub_buffer(MeshBuffer::SubBufferType::COLOR, "triangles"));

            move_vec(local_immediate_data.lines,
                buffer.get_sub_buffer(MeshBuffer::SubBufferType::POSITION, "lines"));
            move_color(local_immediate_data.lines_colors,
                buffer.get_sub_buffer(MeshBuffer::SubBufferType::COLOR, "lines"));

            GLScope gl;
            fbo.bind();
            
            fbo.resize_attachments(int(camera.get_window_width()), int(camera.get_window_height()));
            gl(glEnable, GL_BLEND);
            gl(glDisable, GL_CULL_FACE);
            gl(glViewport, 0, 0, int(camera.get_window_width()), int(camera.get_window_height()));
            


            Eigen::Matrix4f I = Eigen::Matrix4f::Identity();
            gl(glEnable, GL_DEPTH_CLAMP);
            gl(glDepthFunc, GL_LEQUAL);

            if (!opt.get<bool>("Depth Test")) {
                gl(glDisable, GL_DEPTH_TEST);
            }

            utils::render::set_render_layer(gl, ++pass_counter);
            gl(glPointSize, opt.get<float>("Point Size"));

            shader.bind();
            shader["PV"] = camera.get_PV();
            shader["NMat"] = I;
            shader["cameraPos"] = camera.get_position();

            shader["M"] = I;
            shader["NMat"] = I;
            shader["has_color_attrib"] = true;
            shader["uniform_color"] = Color(1, 1, 1, 1).to_vec4();

            
            buffer.render(MeshBuffer::Primitive::TRIANGLES,
                {{MeshBuffer::SubBufferType::POSITION, "triangles"},
                    {MeshBuffer::SubBufferType::COLOR, "triangles"}});
            buffer.render(MeshBuffer::Primitive::LINES,
                {{MeshBuffer::SubBufferType::POSITION, "lines"},
                    {MeshBuffer::SubBufferType::COLOR, "lines"}});
            buffer.render(MeshBuffer::Primitive::POINTS,
                {{MeshBuffer::SubBufferType::POSITION, "points"},
                    {MeshBuffer::SubBufferType::COLOR, "points"}});
        });

    pass->add_tag("default");
    pass->add_tag("immediate");
    pass->add_tag("post");

    return pass;
}

} // namespace ui
} // namespace lagrange

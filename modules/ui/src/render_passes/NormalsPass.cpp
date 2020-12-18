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
#include <lagrange/ui/RenderResourceBuilder.h>
#include <lagrange/ui/Scene.h>
#include <lagrange/ui/render_passes/NormalsPass.h>


namespace lagrange {
namespace ui {

template <>
std::string default_render_pass_name<NormalsPass>()
{
    return "NormalsPass";
}


std::unique_ptr<NormalsPass> create_normals_pass(Resource<CommonPassData> common)
{
    auto pass = std::make_unique<NormalsPass>(
        default_render_pass_name<NormalsPass>(),
        [=](NormalsPassData& data, OptionSet& opt, RenderResourceBuilder& builder) {
            data.common = common;
            data.shader_triangle = builder.create<Shader>("triangle_normals",
                ShaderResourceParams{
                    ShaderResourceParams::Tag::VIRTUAL_PATH,
                    "normals/triangle.shader"});

            data.shader_vertex = builder.create<Shader>(
                "vertex_normals",
                ShaderResourceParams{
                    ShaderResourceParams::Tag::VIRTUAL_PATH,
                    "normals/vertex.shader"});

            opt.add<float>("Length", 0.1f);

            opt["Corner Vertex"].add<bool>("Enabled", false);
            opt["Corner Vertex"].add<Color>("Color", Color(1.0f, 0.0f, 1.0f, 1.0f));
            opt["Corner Vertex"].add<bool>("Use normal's color", true);

            opt["Per-Vertex"].add<bool>("Enabled", false);
            opt["Per-Vertex"].add<Color>("Color", Color(0.92f, 0.57f, 0.2f, 1.0f));
            opt["Per-Vertex"].add<bool>("Use normal's color", true);

            opt["Face"].add<bool>("Enabled", true);
            opt["Face"].add<Color>("Color", Color(0.0f, 1.0f, 0.0f, 1.0f));
            opt["Face"].add<bool>("Use normal's color", true);
        },
        [](const NormalsPassData& data, const OptionSet& opt) {
            auto& fbo = *data.common->final_output_fbo;
            auto& camera = *data.common->camera;
            const auto& scene = *data.common->scene;

            auto _render_objects = [&](Shader& shader,
                                        const std::string & indexing,
                                       MeshBuffer::Primitive primitive) {
                for (auto& model_ptr : scene.get_models()) {
                    auto& model = *model_ptr;
                    auto buffer = model.get_buffer();
                    if (!buffer) return;

                    if (!model.is_visible()) continue;

                    GLScope gl_object;
                    auto object_cam = camera.transformed(model.get_viewport_transform());
                    gl_object(glViewport,
                        int(object_cam.get_window_origin().x()),
                        int(object_cam.get_window_origin().y()),
                        int(object_cam.get_window_width()),
                        int(object_cam.get_window_height()));
                    shader["PV"] = object_cam.get_PV();
                    shader["M"] = model.get_transform();
                    shader["NMat"] = normal_matrix(model.get_transform());

                    buffer->render(primitive, {{MeshBuffer::SubBufferType::INDICES, indexing}});
                }
            };

            GLScope gl;
            fbo.bind();

            gl(glDisable, GL_MULTISAMPLE);
            gl(glEnable, GL_DEPTH_TEST);
            gl(glDepthMask, GL_FALSE);
            gl(glDepthFunc, GL_LEQUAL);
            gl(glEnable, GL_BLEND);
            gl(glViewport, 0, 0, int(camera.get_window_width()), int(camera.get_window_height()));

            /*
                Per-Vertex normals
                Cached in vertex buffer
            */
            if (opt["Per-Vertex"].get<bool>("Enabled")) {
                auto& pervertex_opt = opt["Per-Vertex"];
                auto& shader = *data.shader_vertex;
                shader.bind();
                shader["color"] = pervertex_opt.get<Color>("Color").to_vec4();
                shader["use_direction_color"] = pervertex_opt.get<bool>("Use normal's color");
                shader["line_length"] = opt.get<float>("Length");

                _render_objects(shader, MeshBuffer::vertex_index_id(), MeshBuffer::Primitive::POINTS);
            }

            if (opt["Corner Vertex"].get<bool>("Enabled")) {
                auto& cornervertex_opt = opt["Corner Vertex"];

                auto& shader = *data.shader_vertex;
                shader.bind();
                shader["color"] = cornervertex_opt.get<Color>("Color").to_vec4();
                shader["use_direction_color"] = cornervertex_opt.get<bool>("Use normal's color");
                shader["line_length"] = opt.get<float>("Length");

                _render_objects(
                    shader, MeshBuffer::corner_index_id(), MeshBuffer::Primitive::POINTS);
            }

            /*
                Face normals
                Generated on the fly using geom shader
            */
            if (opt["Face"].get<bool>("Enabled")) {
                auto& face_opt = opt["Face"];

                auto& shader = *data.shader_triangle;
                shader.bind();
                shader["color"] = face_opt.get<Color>("Color").to_vec4();
                shader["use_direction_color"] = face_opt.get<bool>("Use normal's color");
                shader["line_length"] = opt.get<float>("Length");
                _render_objects(
                    shader, MeshBuffer::facet_index_id(), MeshBuffer::Primitive::TRIANGLES);
            }
        });

    pass->add_tag("default");
    pass->add_tag("normal");
    pass->add_tag("post");

    return pass;
}

} // namespace ui
} // namespace lagrange

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
#include <lagrange/ui/IBL.h>
#include <lagrange/ui/MeshBufferFactory.h>
#include <lagrange/ui/RenderResourceBuilder.h>
#include <lagrange/ui/Scene.h>
#include <lagrange/ui/render_passes/SkyboxPass.h>

namespace lagrange {
namespace ui {

template <>
std::string default_render_pass_name<SkyboxPass>()
{
    return "SkyboxPass";
}


std::unique_ptr<SkyboxPass> create_skybox_pass(Resource<CommonPassData> common)
{
    auto pass = std::make_unique<SkyboxPass>(
        default_render_pass_name<SkyboxPass>(),
        [=](SkyboxPassData& data, OptionSet& options, RenderResourceBuilder& builder) {
            data.shader = builder.create<Shader>(
                "skybox",
                ShaderResourceParams{ShaderResourceParams::Tag::VIRTUAL_PATH, "skybox.shader"});

            data.common = common;

            options.add<float>("blur", 1.0f, 0.0f, 10.0f, 1.0f);
            options.add<Eigen::Vector4f>(
                "background",
                Eigen::Vector4f(2.2f, 2.2f, 2.2f, 1.0f),
                Eigen::Vector4f::Constant(0),
                Eigen::Vector4f::Constant(100.0f),
                Eigen::Vector4f::Constant(0.1f));
            options.add<bool>("show_skybox", true);
        },
        [](const SkyboxPassData& data, const OptionSet& options) {
            auto& fbo = *data.common->final_output_fbo;
            auto& shader = *data.shader;
            auto& camera = *data.common->camera;
            auto& scene = *data.common->scene;

            auto& cube = MeshBuffer::cube();

            float blur = options.get<float>("blur");
            auto bgcolor = options.get<Eigen::Vector4f>("background");
            bool show_skybox = options.get<bool>("show_skybox");

            GLScope gl;
            fbo.bind();
            shader.bind();


            // Setup phase
            {
                fbo.resize_attachments(
                    int(camera.get_window_width()),
                    int(camera.get_window_height()));

                gl(glEnable, GL_DEPTH_TEST);
                gl(glDepthMask, GL_FALSE);
                gl(glDepthFunc, GL_LEQUAL);

                gl(glDisable, GL_MULTISAMPLE);
                gl(glEnable, GL_BLEND);


                gl(glViewport,
                   0,
                   0,
                   int(camera.get_window_width()),
                   int(camera.get_window_height()));


                gl(glClearColor, bgcolor.x(), bgcolor.y(), bgcolor.z(), bgcolor.w());
                gl(glClear, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            }

            if (fbo.is_srgb()) {
                gl(glEnable, GL_FRAMEBUFFER_SRGB);
            }

            if (show_skybox) {
                gl(glFrontFace, GL_CW);

                for (auto& emitter_ptr : scene.get_emitters()) {
                    if (!emitter_ptr->is_enabled()) continue;
                    auto& emitter = *emitter_ptr;

                    if (emitter.get_type() == Emitter::Type::IBL) {
                        auto& ibl = reinterpret_cast<const IBL&>(emitter);

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

                        if (blur > 0.0f) {
                            ibl.get_specular().bind_to(GL_TEXTURE0 + 0);
                            shader["mip_level"] = blur;
                        } else {
                            ibl.get_background().bind_to(GL_TEXTURE0 + 0);
                        }

                        cube.render(MeshBuffer::Primitive::TRIANGLES);

                        break; // only render first one
                    }
                }
                gl(glFrontFace, GL_CCW);
            }

            shader.unbind();
            fbo.unbind();
        });

    pass->add_tag("default");
    pass->add_tag("pbr");

    return pass;
}

} // namespace ui
} // namespace lagrange

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
#include <lagrange/ui/render_passes/GroundPass.h>

#include <lagrange/ui/Material.h>
#include <lagrange/ui/RenderUtils.h>
#include <lagrange/ui/RenderResourceBuilder.h>
#include <lagrange/ui/Scene.h>


namespace lagrange {
namespace ui {

template <>
std::string default_render_pass_name<GroundPass>()
{
    return "GroundPass";
}


std::unique_ptr<GroundPass> create_grid_pass(
    Resource<CommonPassData> common, ShadowMapPass* shadow_map_pass, BRDFLUTPass* brdflut_pass)
{
    auto pass = std::make_unique<GroundPass>(
        default_render_pass_name<GroundPass>(),
        [=](GroundPassData& data, OptionSet& options, RenderResourceBuilder& builder) {
            data.shader = builder.create<Shader>("procedural/ground.shader",
                ShaderResourceParams{
                    ShaderResourceParams::Tag::VIRTUAL_PATH,
                    "procedural/ground.shader"});

            assert(shadow_map_pass);
            data.emitters = builder.read(shadow_map_pass->get_data().emitters);

            assert(brdflut_pass);
            data.brdflut = builder.read(brdflut_pass->get_data().brdf_lut_output);

            options.add<float>("Height", 0.0f);

            options.add<bool>("Show Surface", true);

            options.add<bool>("Show Grid", false);
            options.add<float>("Grid Period Primary", 20.0f);
            options.add<float>("Grid Period Secondary", 0.5f);
            options.add<Color>("Grid Color", Color(0.0f, 0.0f, 0.0f, 0.05f));

            options.add<bool>("Show Axes", false);

            options.add<bool>("Shadow Catcher", false);
            options.add<float>("Shadow Catcher Opacity", 0.05f);

            data.common = common;
        },
        [](const GroundPassData& data, const OptionSet& options) {
            const auto& camera = *data.common->camera;
            auto& quad = MeshBuffer::quad();
            auto& shader = *data.shader;

            auto& pass_counter = *data.common->pass_counter;
            pass_counter += 1;

            auto& fbo = *data.common->final_output_fbo;


            GLScope gl;
            utils::render::set_render_pass_defaults(gl);
            fbo.bind();


            shader.bind();
            gl(glViewport, 0, 0, int(camera.get_window_width()), int(camera.get_window_height()));
            gl(glDisable, GL_CULL_FACE);
            gl(glEnable, GL_DEPTH_TEST);

            // Layering
            utils::render::set_render_layer(gl, pass_counter);
            Eigen::Matrix4f I = Eigen::Matrix4f::Identity();
            shader["M"] = I;
            shader["PV"] = camera.get_PV();
            shader["NMat"] = I;

            shader["ground_height"] = options.get<float>("Height");

            // Raycasting parameters
            auto wsize = camera.get_window_size();
            shader["screen_size_inv"] = Eigen::Vector2f(1.0f / wsize.x(), 1.0f / wsize.y());
            shader["Pinv"] = camera.get_perspective_inverse();
            shader["Vinv"] = camera.get_view_inverse();
            shader["cameraPos"] = camera.get_position();

            // Grid parameters
            shader["show_grid"] = options.get<bool>("Show Grid");
            shader["period_primary"] = options.get<float>("Grid Period Primary");
            shader["period_secondary"] = options.get<float>("Grid Period Secondary");
            shader["show_axes"] = options.get<bool>("Show Axes");
            shader["grid_color"] = options.get<Color>("Grid Color").to_vec4();

            // PBR material
            shader["render_pbr"] = options.get<bool>("Show Surface");
            shader["shadow_catcher"] = options.get<bool>("Shadow Catcher");
            shader["shadow_catcher_alpha"] = options.get<float>("Shadow Catcher Opacity");

            int tex_units_global = 0;
            utils::render::set_emitter_uniforms(shader, *data.emitters, tex_units_global);
            utils::render::set_pbr_uniforms(shader, *data.brdflut, tex_units_global);

            shader["has_color_attrib"] = false;
            shader["uniform_color"] = Eigen::Vector4f(-1, -1, -1, -1);
            auto& mat = *data.material;
            {
                utils::render::set_map(shader, "baseColor", mat, tex_units_global);
                utils::render::set_map(shader, "roughness", mat, tex_units_global);
                utils::render::set_map(shader, "metallic", mat, tex_units_global);
                utils::render::set_map(shader, "normal", mat, tex_units_global);
                shader["material.opacity"] = mat["opacity"].value.to_vec3().x();
            }

            shader["camera_is_ortho"] = (camera.get_type() == Camera::Type::ORTHOGRAPHIC);

            quad.render(MeshBuffer::Primitive::TRIANGLES);
        });

    pass->add_tag("default");
    pass->add_tag("post");

    return pass;
}

Ground::Ground(GroundPass& pass)
    : m_pass(pass)
{}


Ground& Ground::set_height(float height)
{
    m_pass.get_options().set<float>("Height", height);
    return *this;
}

float Ground::get_height() const
{
    return m_pass.get_options().get<float>("Height");
}

Ground& Ground::enable_surface(bool enable)
{
    if (enable) {
        enable_shadow_catcher(false);
    }
    m_pass.get_options().set<bool>("Show Surface", enable);
    return *this;
}

Ground& Ground::set_surface_material(std::shared_ptr<Material> mat)
{
    assert(mat);
    m_pass.get_data().material = mat;
    return *this;
}

Material& Ground::get_surface_material()
{
    return *m_pass.get_data().material;
}

const Material& Ground::get_surface_material() const
{
    return *m_pass.get_data().material;
}

Ground& Ground::enable_grid(bool enable)
{
    m_pass.get_options().set<bool>("Show Grid", enable);
    return *this;
}

Ground& Ground::set_grid_color(Color color)
{
    m_pass.get_options().set<Color>("Grid Color", color);
    return *this;
}

Color Ground::get_grid_color() const
{
    return m_pass.get_options().get<Color>("Grid Color");
}

Ground& Ground::set_grid_period(float primary, float secondary)
{
    m_pass.get_options().set<float>("Grid Period Primary", primary);
    m_pass.get_options().set<float>("Grid Period Secondary", secondary);
    return *this;
}

Ground& Ground::set_grid_period(std::pair<float, float> period)
{
    set_grid_period(period.first, period.second);
    return *this;
}

std::pair<float, float> Ground::get_grid_period() const
{
    return {m_pass.get_options().get<float>("Grid Period Primary"),
        m_pass.get_options().get<float>("Grid Period Secondary")};
}

Ground& Ground::enable_axes(bool enable)
{
    m_pass.get_options().set<bool>("Show Axes", enable);
    return *this;
}

Ground& Ground::enable_shadow_catcher(bool enable)
{
    if (enable) {
        enable_surface(false);
    }
    m_pass.get_options().set<bool>("Shadow Catcher", enable);
    return *this;
}

Ground& Ground::set_shadow_catcher_opacity(float opacity)
{
    m_pass.get_options().set<float>("Shadow Catcher Opacity", opacity);
    return *this;
}

float Ground::get_shadow_catcher_opacity() const
{
    return m_pass.get_options().get<float>("Shadow Catcher Opacity");
}

} // namespace ui
} // namespace lagrange

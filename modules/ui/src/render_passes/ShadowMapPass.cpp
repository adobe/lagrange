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
#include <lagrange/ui/render_passes/ShadowMapPass.h>

#include <lagrange/ui/MeshModelBase.h>
#include <lagrange/utils/strings.h>
#include <lagrange/utils/utils.h>

namespace lagrange {
namespace ui {

template <>
std::string default_render_pass_name<ShadowMapPass>()
{
    return "ShadowMapPass";
}


std::unique_ptr<ShadowMapPass> create_shadow_map_pass(Resource<CommonPassData> common)
{
    auto pass = std::make_unique<ShadowMapPass>(
        default_render_pass_name<ShadowMapPass>(),
        [=](ShadowMapPassData& data, OptionSet& options, RenderResourceBuilder& builder) {
            // Shadow map 2D

            int shadow_map_counter = 0;
            for (auto& map : data.maps_2D) {
                auto params = Texture::Params::depth();
                params.wrap_r = GL_CLAMP_TO_BORDER;
                params.wrap_s = GL_CLAMP_TO_BORDER;
                params.wrap_t = GL_CLAMP_TO_BORDER;
                params.border_color[0] = 1.0f;
                params.border_color[1] = 1.0f;
                params.border_color[2] = 1.0f;
                params.border_color[3] = 1.0f;

                auto shadow_map_2D = builder.create<Texture>(
                    string_format("_shadow_map_2D_{}", shadow_map_counter++),
                    params);
                map = builder.write(shadow_map_2D);
            }

            // Shadow map cube
            shadow_map_counter = 0;
            for (auto& map : data.maps_cube) {
                Texture::Params p = Texture::Params::depth();
                p.type = GL_TEXTURE_CUBE_MAP;

                auto shadow_map_cube = builder.create<Texture>(
                    string_format("_shadow_map_cube_{}", shadow_map_counter++),
                    p
                    );

                map = builder.write(shadow_map_cube);
            }

            data.emitters = builder.create<std::vector<EmitterRenderData>>("_emitters");
            builder.write(data.emitters);

            data.shader_2D = builder.create<Shader>("depth",
                ShaderResourceParams {ShaderResourceParams::Tag::VIRTUAL_PATH, "depth/to_texture.shader"});
            data.shader_cube = builder.create<Shader>(
                "depth_cube",
                ShaderResourceParams{
                    ShaderResourceParams::Tag::VIRTUAL_PATH,
                    "depth/to_cubemap.shader"});


            options.add<int>("resolution_2D", 1024, 1);
            options.add<int>("resolution_cube", 1024, 1);

            options.add<float>("near", 0.01f);
            options.add<float>("far", 32.00f);

            options.add<Eigen::Vector3f>("min_corner", Eigen::Vector3f::Ones());
            options.add<Eigen::Vector3f>("max_corner", Eigen::Vector3f::Zero());

            data.common = common;
            data.target_fbo = builder.create<FrameBuffer>(
                "_shadow_fbo", FBOResourceParams{{}, data.maps_2D[0]});
        },
        [](const ShadowMapPassData& data, const OptionSet& options) {
            const auto& scene = *data.common->scene;
            const auto& camera = *data.common->camera;

            auto& fbo = *data.target_fbo;


            const auto res_2D = options.get<int>("resolution_2D");
            const auto res_cube = options.get<int>("resolution_cube");

            auto& shader_2D = *data.shader_2D;
            auto& shader_cube = *data.shader_cube;

            const auto shadow_near = options.get<float>("near");
            const auto shadow_far = options.get<float>("far");

            const auto shadow_min_corner = options.get<Eigen::Vector3f>("min_corner");
            const auto shadow_max_corner = options.get<Eigen::Vector3f>("max_corner");

            Eigen::AlignedBox3f shadow_box;
            if ((shadow_min_corner.array() <= shadow_max_corner.array()).all()) {
                shadow_box = {shadow_min_corner, shadow_max_corner};
            }

            auto& emitters = *data.emitters;


            emitters.clear();


            GLScope gl;
            //TODO sort emitters to avoid switching shaders/textures too often

            int counter_cube = 0;
            int counter_2d = 0;
            int counter_total = 0;
            int counter_ibl = 0;

            for (auto& emitter : scene.get_emitters()) {
                if (!emitter->is_enabled()) continue;

                const auto emitter_type = emitter->get_type();
                if (emitter_type == Emitter::Type::IBL) {
                    // Only use first IBL
                    if (counter_ibl > 0) {
                        continue;
                    }
                    emitters.push_back({});
                    auto& emitter_data = emitters.back();
                    emitter_data.emitter = emitter;
                    counter_ibl++;
                    continue;
                }

                // Max total lights reached
                if (counter_total == MAX_DEFAULT_LIGHTS_TOTAL) continue;

                if (emitter_type == Emitter::Type::POINT &&
                    counter_cube == MAX_DEFAULT_LIGHTS_POINT) {
                    logger().warn("reached max number of cube map shadows");
                    continue;
                }
                if (emitter_type != Emitter::Type::POINT &&
                    counter_2d == MAX_DEFAULT_LIGHTS_SPOT_DIR) {
                    logger().warn("reached max number of 2d shadows");
                    continue;
                }

                // Grab next available texture map
                auto map_resource = (emitter_type == Emitter::Type::POINT)
                                        ? data.maps_cube[counter_cube++]
                                        : data.maps_2D[counter_2d++];

                
                // Write shadow map properties
                emitters.push_back({});
                auto& emitter_data = emitters.back();
                emitter_data.shadow_map = map_resource;
                emitter_data.emitter = emitter;

                const auto res = (emitter_type == Emitter::Type::POINT) ? res_cube : res_2D;
                const auto aspect_ratio = 1.0f;

                // Initialize FBO
                fbo.set_depth_attachement(map_resource, map_resource->get_params().type);
                fbo.resize_attachments(res, res);

                // Initialize shader
                auto& shader = (emitter_type == Emitter::Type::POINT) ? shader_cube : shader_2D;
                shader.bind();

                gl(glDrawBuffer, GL_NONE);
                gl(glReadBuffer, GL_NONE);
                gl(glCullFace, GL_FRONT);
                gl(glViewport, 0, 0, res, res);
                gl(glClear, GL_DEPTH_BUFFER_BIT);

                /*
                    Calculate projection matrix/matrices
                */
                if (emitter_type == Emitter::Type::POINT) {
                    const auto& point_light = reinterpret_cast<PointLight&>(*emitter);
                    const auto pos = point_light.get_position();

                    auto P = perspective(
                        lagrange::to_radians(90.0f), aspect_ratio, shadow_near, shadow_far);

                    // clang-format off
                    Eigen::Matrix4f PV[6];
                    PV[0] = P * look_at(pos, pos + Eigen::Vector3f(1, 0, 0), Eigen::Vector3f(0, -1, 0));
                    PV[1] = P * look_at(pos, pos + Eigen::Vector3f(-1, 0, 0), Eigen::Vector3f(0, -1, 0));
                    PV[2] = P * look_at(pos, pos + Eigen::Vector3f(0, 1, 0), Eigen::Vector3f(0, 0, 1));
                    PV[3] = P * look_at(pos, pos + Eigen::Vector3f(0, -1, 0), Eigen::Vector3f(0, 0, -1));
                    PV[4] = P * look_at(pos, pos + Eigen::Vector3f(0, 0, 1), Eigen::Vector3f(0, -1, 0));
                    PV[5] = P * look_at(pos, pos + Eigen::Vector3f(0, 0, -1), Eigen::Vector3f(0, -1, 0));
                    // clang-format on

                    shader["shadowPV"].set_matrices(PV, 6);
                    emitter_data.PV = Eigen::Matrix4f::Identity();

                    shader["originPos"] = pos;
                } else if (emitter_type == Emitter::Type::SPOT) {
                    const auto& spot = reinterpret_cast<SpotLight&>(*emitter);

                    float angle = spot.get_cone_angle() * 2;
                    auto pos = spot.get_position();
                    auto dir = spot.get_direction();

                    auto P = perspective(angle, aspect_ratio, shadow_near, shadow_far);
                    const auto plane = spot.get_perpendicular_plane();

                    auto V = look_at(pos, pos + dir, plane.first);

                    Eigen::Matrix4f PV = (P * V);
                    emitter_data.PV = PV;
                    shader["PV"] = PV;
                    shader["originPos"] = pos;
                } else if (emitter_type == Emitter::Type::DIRECTIONAL) {
                    const auto& dir_light = reinterpret_cast<DirectionalLight&>(*emitter);

                    const auto dir = dir_light.get_direction();
                    const auto plane = dir_light.get_perpendicular_plane();

                    if (shadow_box.isEmpty()) {
                        float range = (shadow_far - shadow_near) / 2.0f;
                        auto P = ortho(-range, range, -range, range, shadow_near, shadow_far);

                        Eigen::Vector3f pos = (camera.get_lookat() - dir * shadow_far * 0.5f);
                        auto V = look_at(pos, pos + dir, plane.first);

                        Eigen::Matrix4f PV = (P * V);
                        emitter_data.PV = PV;
                        shader["PV"] = PV;
                        shader["originPos"] = pos;
                    } else {
                        // Transformation to light space
                        Eigen::Matrix3f dir_proj;
                        dir_proj.col(0) = plane.first;
                        dir_proj.col(1) = plane.second;
                        dir_proj.col(2) = dir;

                        // Transform shadow box to light space and find its AABB
                        AABB proj_bbox;
                        for (int i = 0; i < 8; ++i) {
                            auto p =
                                shadow_box.corner(static_cast<Eigen::AlignedBox3f::CornerType>(i));
                            proj_bbox.extend(dir_proj * p);
                        }

                        // Shadow map render position in light space
                        Eigen::Vector3f cam_proj_center = Eigen::Vector3f(
                            proj_bbox.center().x(), proj_bbox.center().y(), proj_bbox.min().z());

                        // Shadow map render position in world space
                        Eigen::Vector3f cam_reproj_center = dir_proj.inverse() * cam_proj_center;

                        // Projection
                        Eigen::Vector3f range = proj_bbox.diagonal();
                        auto P = ortho(-range.x() * 0.5f,
                            range.x() * 0.5f,
                            -range.y() * 0.5f,
                            range.y() * 0.5f,
                            0,
                            range.z());

                        // View
                        auto V = look_at(cam_reproj_center, cam_reproj_center + dir, plane.second);

                        Eigen::Matrix4f PV = (P * V);
                        emitter_data.PV = PV;
                        shader["PV"] = PV;
                        shader["originPos"] = cam_reproj_center;
                    }
                }

                shader["near"] = shadow_near;
                shader["far"] = shadow_far;

                emitter_data.shadow_far = shadow_far;
                emitter_data.shadow_near = shadow_near;

                // Render objects from light's point of view
                for (auto model_ptr : scene.get_models()) {
                    if (data.model_selection.size() > 0) {
                        if (!data.model_selection.has(model_ptr)) continue;
                    }

                    auto mesh_model_ptr = dynamic_cast<MeshModelBase*>(model_ptr);
                    if (!mesh_model_ptr) continue;

                    auto& mesh_model = *mesh_model_ptr;

                    
                    if (!mesh_model.is_visible()) continue;

                    auto buffer = mesh_model.get_buffer();
                    if (!buffer) continue;

                    shader["M"] = mesh_model.get_transform();

                    buffer->render(MeshBuffer::Primitive::TRIANGLES);
                }
            }
        });

    pass->add_tag("default");
    pass->add_tag("pbr");

    return pass;
}

} // namespace ui
} // namespace lagrange

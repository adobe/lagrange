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

#include <lagrange/ui/systems/render_shadowmaps.h>

#include <lagrange/ui/components/Common.h>
#include <lagrange/ui/components/Light.h>
#include <lagrange/ui/components/RenderContext.h>
#include <lagrange/ui/components/ShadowMap.h>
#include <lagrange/ui/components/Transform.h>
#include <lagrange/ui/default_shaders.h>
#include <lagrange/ui/systems/render_geometry.h>
#include <lagrange/ui/types/Material.h>
#include <lagrange/ui/utils/bounds.h>
#include <lagrange/ui/utils/layer.h>
#include <lagrange/ui/utils/lights.h>
#include <lagrange/ui/utils/render.h>
#include <lagrange/utils/utils.h>


namespace lagrange {
namespace ui {

struct ShadowMapFBO
{
    std::shared_ptr<FrameBuffer> fbo;
};

struct ShadowmappingViewport : public ViewportComponent
{
};

void update_shadowmap_texture_cube(ShadowMap& shadowmap)
{
    if (!shadowmap.texture) {
        Texture::Params p = Texture::Params::depth();
        p.type = GL_TEXTURE_CUBE_MAP;
        shadowmap.texture = std::make_shared<Texture>(p);
    }
}

void update_shadowmap_texture_2D(ShadowMap& shadowmap)
{
    if (!shadowmap.texture) {
        auto params = Texture::Params::depth();
        params.wrap_r = GL_CLAMP_TO_BORDER;
        params.wrap_s = GL_CLAMP_TO_BORDER;
        params.wrap_t = GL_CLAMP_TO_BORDER;
        params.border_color[0] = 1.0f;
        params.border_color[1] = 1.0f;
        params.border_color[2] = 1.0f;
        params.border_color[3] = 1.0f;
        shadowmap.texture = std::make_shared<Texture>(params);
    }
}


void render_shadowmaps(Registry& r)
{
    // Add viewport and shadowmap to lights if do not exist
    r.view<LightComponent>().each([&](Entity e, LightComponent& light) {
        if (light.casts_shadow) {
            if (!r.has<ShadowMap>(e)) r.emplace<ShadowMap>(e);
            if (!r.has<ViewportComponent>(e)) r.emplace<ViewportComponent>(e);
        } else {
            if (r.has<ShadowMap>(e)) r.remove<ShadowMap>(e);
            if (r.has<ViewportComponent>(e)) r.remove<ViewportComponent>(e);
        }
    });

    r.view<LightComponent, ShadowMap, ViewportComponent>().each([&](Entity e,
                                                                    const LightComponent& light,
                                                                    ShadowMap& shadowmap,
                                                                    ViewportComponent& viewport) {
        // Update shadowmap viewport size
        viewport.width = shadowmap.resolution;
        viewport.height = shadowmap.resolution;

        // Don't render objects in NoShadow layer
        viewport.hidden_layers.set(DefaultLayers::NoShadow, true);


        // Setup material override
        //TODO: special case for dynamic geometry (e.g. skinned/animated meshes)

        auto [pos, dir] = get_light_position_and_direction(r, e);

        if (light.type == LightComponent::Type::POINT) {
            if (!viewport.material_override) {
                viewport.material_override =
                    std::make_shared<Material>(r, DefaultShaders::ShadowCubemap);
            }
            update_shadowmap_texture_cube(shadowmap);
        } else {
            if (!viewport.material_override) {
                viewport.material_override =
                    std::make_shared<Material>(r, DefaultShaders::ShadowDepth);
            }
            update_shadowmap_texture_2D(shadowmap);
        }

        viewport.material_override->set_float("near", shadowmap.near_plane);
        viewport.material_override->set_float("far", shadowmap.far_plane);


        // Shader uniforms
        if (light.type == LightComponent::Type::POINT) {
            const auto aspect_ratio = 1.0f;
            auto P = perspective(
                lagrange::to_radians(90.0f),
                aspect_ratio,
                shadowmap.near_plane,
                shadowmap.far_plane);

            // clang-format off
                    Eigen::Matrix4f PV[6];
                    PV[0] = P * look_at(pos, pos + Eigen::Vector3f(1, 0, 0), Eigen::Vector3f(0,-1, 0)); 
                    PV[1] = P * look_at(pos, pos + Eigen::Vector3f(-1, 0, 0), Eigen::Vector3f(0, -1, 0));
                    PV[2] = P * look_at(pos, pos + Eigen::Vector3f(0, 1, 0), Eigen::Vector3f(0, 0, 1)); 
                    PV[3] = P * look_at(pos, pos + Eigen::Vector3f(0, -1, 0), Eigen::Vector3f(0, 0, -1));
                    PV[4] = P * look_at(pos, pos + Eigen::Vector3f(0, 0, 1), Eigen::Vector3f(0, -1, 0)); 
                    PV[5] = P * look_at(pos, pos + Eigen::Vector3f(0, 0, -1), Eigen::Vector3f(0, -1, 0));
            // clang-format on

            viewport.material_override->set_mat4("PV", Eigen::Matrix4f::Identity());
            viewport.material_override->set_mat4_array("shadowPV", PV, 6);
            viewport.material_override->set_vec4(
                "originPos",
                Eigen::Vector4f(pos.x(), pos.y(), pos.z(), 1.0f));

        } else if (light.type == LightComponent::Type::DIRECTIONAL) {
            Eigen::Matrix4f PV;

            const Eigen::AlignedBox3f shadow_box = get_scene_bounds(r).global;
            if (!shadow_box.isEmpty()) {
                const auto plane = utils::render::compute_perpendicular_plane(dir);
                // Transformation to light space
                Eigen::Matrix3f dir_proj;
                dir_proj.col(0) = plane.first;
                dir_proj.col(1) = plane.second;
                dir_proj.col(2) = dir;

                // Transform shadow box to light space and find its AABB
                AABB proj_bbox;
                for (int i = 0; i < 8; ++i) {
                    auto p = shadow_box.corner(static_cast<Eigen::AlignedBox3f::CornerType>(i));
                    proj_bbox.extend(dir_proj * p);
                }

                // Shadow map render position in light space
                Eigen::Vector3f cam_proj_center = Eigen::Vector3f(
                    proj_bbox.center().x(),
                    proj_bbox.center().y(),
                    proj_bbox.min().z());

                // Shadow map render position in world space
                Eigen::Vector3f cam_reproj_center = dir_proj.inverse() * cam_proj_center;

                // Projection
                Eigen::Vector3f range = proj_bbox.diagonal();
                auto P = ortho(
                    -range.x() * 0.5f,
                    range.x() * 0.5f,
                    -range.y() * 0.5f,
                    range.y() * 0.5f,
                    0,
                    range.z());

                // View
                auto V = look_at(cam_reproj_center, cam_reproj_center + dir, plane.second);

                PV = (P * V);
                pos = cam_reproj_center;
                shadowmap.PV = PV;
            }

            viewport.material_override->set_mat4("PV", PV);
            viewport.material_override->set_vec4(
                "originPos",
                Eigen::Vector4f(pos.x(), pos.y(), pos.z(), 1.0f));

        } else if (light.type == LightComponent::Type::SPOT) {
            Eigen::Matrix4f PV;
            {
                const float angle = light.cone_angle * 2;
                const auto P = perspective(angle, 1.0f, shadowmap.near_plane, shadowmap.far_plane);
                const auto plane = utils::render::compute_perpendicular_plane(dir);
                const auto V = look_at(pos, pos + dir, plane.first);
                PV = P * V;
                shadowmap.PV = PV;
            }
            viewport.material_override->set_mat4("PV", PV);
        }

        if (!viewport.fbo) {
            viewport.fbo = std::make_shared<FrameBuffer>();
        }

        auto& fbo = *viewport.fbo;
        fbo.set_depth_attachement(shadowmap.texture, shadowmap.texture->get_params().type);
        fbo.resize_attachments(shadowmap.texture->get_width(), shadowmap.texture->get_height());
        viewport.material_override->set_int(RasterizerOptions::DrawBuffer, GL_NONE);
        viewport.material_override->set_int(RasterizerOptions::ReadBuffer, GL_NONE);
        viewport.material_override->set_int(RasterizerOptions::CullFace, GL_FRONT);
        viewport.material_override->set_int(RasterizerOptions::CullFaceEnabled, GL_TRUE);
    });
}

} // namespace ui
} // namespace lagrange
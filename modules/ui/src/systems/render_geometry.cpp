/*
 * Copyright 2021 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/fs/file_utils.h>
#include <lagrange/ui/components/AttributeRender.h>
#include <lagrange/ui/components/Bounds.h>
#include <lagrange/ui/components/IBL.h>
#include <lagrange/ui/components/Light.h>
#include <lagrange/ui/components/MeshData.h>
#include <lagrange/ui/components/MeshGeometry.h>
#include <lagrange/ui/components/MeshRender.h>
#include <lagrange/ui/components/RenderContext.h>
#include <lagrange/ui/components/ShadowMap.h>
#include <lagrange/ui/components/Transform.h>
#include <lagrange/ui/default_shaders.h>
#include <lagrange/ui/systems/render_geometry.h>
#include <lagrange/ui/types/Shader.h>
#include <lagrange/ui/types/ShaderLoader.h>
#include <lagrange/ui/utils/layer.h>
#include <lagrange/ui/utils/lights.h>
#include <lagrange/ui/utils/logger.h>
#include <lagrange/ui/utils/mesh.h>
#include <lagrange/ui/utils/render.h>

namespace lagrange {
namespace ui {


void setup_vertex_data(Registry& r)
{
    // Emplace GLVertexData to MeshRender -> OpenGL specific vertex attribute array info for
    // rendering
    r.view<MeshRender>().each([&](Entity e, const MeshRender& render) {
        if (!render.material) return;
        if (!r.all_of<VertexData>(e)) r.emplace<VertexData>(e);
    });


    // Setup attribute renders
    r.view<VertexData, MeshRender, AttributeRender, MeshGeometry>().each([&](Entity /*e*/,
                                                                             VertexData& glvd,
                                                                             MeshRender& render,
                                                                             AttributeRender& ar,
                                                                             MeshGeometry& mg) {
        if (!r.valid(mg.entity)) return;

        auto& md = r.get<MeshData>(mg.entity);

        auto shader_handle = ui::get_shader(r, render.material->shader_id());
        if (!shader_handle) return;

        auto& shader = *shader_handle;

        auto it = shader.attribs().find(DefaultShaderAtrribNames::Value);
        if (it == shader.attribs().end()) return;


        // Up to date, skip
        if (!ar.dirty && glvd.attribute_buffers[it->second.location]) return;

        // Check if attribute exists
        if (ar.attribute_type == IndexingMode::CORNER &&
            !has_mesh_corner_attribute(md, ar.source_attribute)) {
            lagrange::logger().warn("Missing corner attribute {}", ar.source_attribute);
            return;
        } else if (
            ar.attribute_type == IndexingMode::FACE &&
            !has_mesh_facet_attribute(md, ar.source_attribute)) {
            lagrange::logger().warn("Missing facet attribute {}", ar.source_attribute);
            return;
        } else if (
            ar.attribute_type == IndexingMode::EDGE &&
            !has_mesh_edge_attribute(md, ar.source_attribute)) {
            lagrange::logger().warn("Missing edge attribute {}", ar.source_attribute);
            return;
        } else if (
            ar.attribute_type == IndexingMode::VERTEX &&
            !has_mesh_vertex_attribute(md, ar.source_attribute)) {
            lagrange::logger().warn("Missing vertex attribute {}", ar.source_attribute);
            return;
        } else if (
            ar.attribute_type == IndexingMode::INDEXED &&
            !has_mesh_indexed_attribute(md, ar.source_attribute)) {
            lagrange::logger().warn("Missing indexed attribute {}", ar.source_attribute);
            return;
        }

        // Get or create gpu buffer
        auto& buf_ptr = glvd.attribute_buffers[it->second.location];
        if (!buf_ptr) {
            buf_ptr = std::make_shared<GPUBuffer>();
        }

        // Retrieve from mesh (and convert to float)
        auto data = get_mesh_attribute(md, ar.attribute_type, ar.source_attribute);

        // Upload to GPU
        switch (ar.attribute_type) {
        case IndexingMode::VERTEX: upload_mesh_vertex_attribute(md, data, *buf_ptr); break;
        case IndexingMode::EDGE: upload_mesh_edge_attribute(md, data, *buf_ptr); break;
        case IndexingMode::FACE: upload_mesh_facet_attribute(md, data, *buf_ptr); break;
        case IndexingMode::CORNER: upload_mesh_corner_attribute(md, data, *buf_ptr); break;
        default: break;
        }

        const int element_size = std::max(1, std::min(4, int(data.cols())));
        glvd.attribute_dimensions[it->second.location] = element_size;

        // Mark up to date
        ar.dirty = false;

        lagrange::logger().trace(
            "Retrieving and uploading attribute '{}', binding to shader location: {}. "
            "Size: {} x {} -> {} x {}",
            ar.source_attribute,
            it->second.location,
            data.rows(),
            data.cols(),
            data.rows(),
            element_size);
    });

    // Setup default mesh rendering attribute arrays
    r.view<VertexData, MeshRender, MeshGeometry>().each(
        [&](Entity /*e*/, VertexData& glvd, MeshRender& render, const MeshGeometry& geom_entity) {
            if (!r.valid(geom_entity.entity)) return;
            if (!r.all_of<GLMesh>(geom_entity.entity)) return;
            if (!render.material) return;

            auto shader_handle = ui::get_shader(r, render.material->shader_id());
            if (!shader_handle) return;

            auto& glmesh = r.get<GLMesh>(geom_entity.entity);
            auto& shader = *shader_handle;

            update_vertex_data(glmesh, shader, glvd, render.indexing, geom_entity.submesh_index);
        });

    r.view<VertexData, MeshRender, GLMesh>().each(
        [&](Entity e, VertexData& glvd, MeshRender& render, GLMesh& glmesh) {
            if (r.all_of<MeshGeometry>(e)) {
                return;
            }
            if (!render.material) return;
            auto shader_handle = ui::get_shader(r, render.material->shader_id());
            if (!shader_handle) return;
            update_vertex_data(glmesh, *shader_handle, glvd, render.indexing, entt::null);
        });
}


std::shared_ptr<Texture> get_brdflut(Registry& r)
{
    struct BRDFLUT
    {
        std::shared_ptr<Texture> texture;
    };


    auto& cached = r.ctx().emplace<BRDFLUT>();
    if (cached.texture) return cached.texture;

    auto shader = ShaderLoader{}("util/brdf_lut.shader", ShaderLoader::PathType::VIRTUAL);
    FrameBuffer temp_fbo;

    auto tex = std::make_shared<Texture>(Texture::Params::rgb());
    temp_fbo.bind();
    temp_fbo.set_color_attachement(0, tex);
    temp_fbo.resize_attachments(512, 512);

    GLScope gl;
#if !defined(__EMSCRIPTEN__)
    // TODO WebGL: GL_MULTISAMPLE is not supported.
    gl(glDisable, GL_MULTISAMPLE);
#endif
    gl(glDisable, GL_DEPTH_TEST);
    gl(glDisable, GL_BLEND);
    gl(glDisable, GL_CULL_FACE);

    gl(glViewport, 0, 0, 512, 512);
    gl(glClearColor, 0.0f, 0.0f, 0.0f, 0.0f);
    gl(glClear, GL_COLOR_BUFFER_BIT);

    shader->bind();

    render_vertex_data(*generate_quad_vertex_data(), GL_TRIANGLES, 3); // Render temporary quad

    shader->unbind();


    cached.texture = tex;
    return cached.texture;
}

void activate_shader(Registry& r, Shader& shader, RenderContext& rctx)
{
    auto gl_c = GLScope::current();

    // Move up a layer with every shader switch
    // utils::render::set_render_layer(gl_c, rctx.polygon_offset_layer++);
    rctx.polygon_offset_layer++;

    shader.bind();

    // Assign all texture units, even if they are not bound to
    for (auto& it : shader.sampler_indices()) {
        shader.uniform(it.first) = it.second;
    }

    // Upload all default values specified by shader *properties*
    shader.upload_default_values();


    // Set IBL
    auto ibls = r.view<IBL>();
    auto& viewport = r.get<ViewportComponent>(rctx.viewport);
    if (ibls.size() > 0 &&
        ui::is_visible_in(r, ibls.front(), viewport.visible_layers, viewport.hidden_layers)) {
        auto& ibl = r.get<IBL>(ibls.front());
        {
            auto it = shader.sampler_indices().find("ibl_diffuse"_hs);
            if (it != shader.sampler_indices().end())
                ibl.diffuse->bind_to(GL_TEXTURE0 + it->second);
        }
        {
            auto it = shader.sampler_indices().find("ibl_specular"_hs);
            if (it != shader.sampler_indices().end())
                ibl.specular->bind_to(GL_TEXTURE0 + it->second);
        }

        shader["has_ibl"] = true;
    } else {
        shader["has_ibl"] = false;
    }

    // Set Lights
    auto lights = r.view<LightComponent>();

    // Sort for consistency
    r.sort<LightComponent>([&](Entity a, Entity b) {
        const auto atype = r.get<LightComponent>(a).type;
        const auto btype = r.get<LightComponent>(b).type;
        if (atype == btype) return a < b;
        return atype < btype;
    });
    // Hard coded for now
    const int max_spot = 2;
    const int max_dir = 2;
    const int max_point = 1;
    int num_spot = 0;
    int num_point = 0;
    int num_dir = 0;


    const auto& sampler_indices = shader.sampler_indices();

    for (auto e : lights) {
        const auto& light = lights.get<LightComponent>(e);

        const auto* shadow_map = r.try_get<ShadowMap>(e);

        auto [pos, dir] = get_light_position_and_direction(r, e);


        std::string prefix = "";
        std::string prefix_name_only = "";
        int num = 0;


        if (light.type == LightComponent::Type::POINT) {
            if (num_point == max_point) continue;
            num = num_point;
            prefix = string_format("point_lights[{}].", num_point);
            prefix_name_only = "point_lights";
            shader[prefix + "position"] = pos;


            num_point++;
        } else if (light.type == LightComponent::Type::DIRECTIONAL) {
            if (num_dir == max_dir) continue;
            prefix = string_format("directional_lights[{}].", num_dir);
            prefix_name_only = "directional_lights";
            num = num_dir;

            if (shadow_map) {
                shader[prefix + "PV"] = shadow_map->PV;
            }

            shader[prefix + "direction"] = dir;
            num_dir++;

        } else if (light.type == LightComponent::Type::SPOT) {
            if (num_spot == max_spot) continue;
            prefix = string_format("spot_lights[{}].", num_spot);
            prefix_name_only = "spot_lights";
            num = num_spot;

            shader[prefix + "position"] = pos;
            shader[prefix + "direction"] = dir;
            shader[prefix + "cone_angle"] = light.cone_angle;

            if (shadow_map) {
                shader[prefix + "PV"] = shadow_map->PV;
            }

            num_spot++;
        } else {
            continue;
        }

        shader[prefix + "intensity"] = light.intensity;
        if (shadow_map) {
            auto sampler_uniform_identifier =
                string_format("{}_shadow_maps_{}", prefix_name_only, num);
            auto it = sampler_indices.find(string_id(sampler_uniform_identifier));
            if (it != sampler_indices.end()) {
                shadow_map->texture->bind_to(GL_TEXTURE0 + it->second);
            }
            shader[prefix + "shadow_near"] = shadow_map->near_plane;
            shader[prefix + "shadow_far"] = shadow_map->far_plane;
        }
    }

#if defined(__EMSCRIPTEN__)
    if (num_point > 0) {
        log_error_once(r, "Point lights are currently not implemented for Emscripten/WebGL");
    }
#endif

    shader["spot_lights_count"] = num_spot;
    shader["point_lights_count"] = num_point;
    shader["directional_lights_count"] = num_dir;


    {
        auto it = shader.sampler_indices().find("ibl_brdf_lut"_hs);
        if (it != shader.sampler_indices().end()) get_brdflut(r)->bind_to(GL_TEXTURE0 + it->second);
    }
}

struct GLRenderQueueItem
{
    int pass = 0;
    Shader* shader;
    Material* material;
    VertexData* glvd;
    PrimitiveType primitive;
    Entity entity;
    bool opaque = false; // For now assume everything is transparent
    float camera_distance = 0.0f;
};

using GLRenderQueue = std::vector<GLRenderQueueItem>;


void meshrender_to_render_queue(Registry& r)
{
    auto& queue = r.ctx().emplace<GLRenderQueue>();

    // auto& rctx = w.ctx().emplace<RenderContext>();
    const auto& viewport = get_render_context_viewport(r);
    const auto& camera = get_viewport_camera(r, viewport);
    auto cam_pos = camera.get_position();

    queue.clear();
    {
        auto mview = r.view<MeshRender, VertexData>();
        queue.reserve(mview.size_hint());

        // TODO: Occlusion culling would go here

        // Fill queue
        mview.each([&](Entity e, MeshRender& mr, VertexData& glvd) {
            // Skip entities no in visible layers and those in hidden layers
            if (!is_visible_in(r, e, viewport.visible_layers, viewport.hidden_layers)) return;

            auto* mat = mr.material.get();
            if (viewport.material_override) {
                mat = viewport.material_override.get();
                // TODO check vertex attrib compatibility?
            }

            if (!mat) return;

            auto shader_handle = ui::get_shader(r, mat->shader_id());
            if (!shader_handle) return;

            auto& shader = *shader_handle;


            GLRenderQueueItem item;

            item.pass = 0;
            auto pass_it = mat->int_values.find(RasterizerOptions::Pass);
            if (pass_it != mat->int_values.end()) {
                item.pass = pass_it->second;
            }

            item.shader = &shader;
            item.material = mat;
            item.glvd = &glvd;
            item.primitive = mr.primitive;
            item.entity = e;

            if (r.all_of<Bounds>(e)) {
                item.camera_distance = (r.get<Bounds>(e).global.center() - cam_pos).norm();
            }


            queue.emplace_back(std::move(item));
        });
    }

    la_debug_assert(r.ctx().contains<GLRenderQueue>());
}

void sort_gl_render_queue(Registry& r)
{
    auto& queue = r.ctx().emplace<GLRenderQueue>();

    std::sort(
        queue.begin(),
        queue.end(),
        [](const GLRenderQueueItem& a, const GLRenderQueueItem& b) {
            const float a_cam_dist_inverse = -a.camera_distance;
            const float b_cam_dist_inverse = -b.camera_distance;

            return std::tie(a.pass, a.shader, a_cam_dist_inverse, a.material, a.glvd) <
                   std::tie(b.pass, b.shader, b_cam_dist_inverse, b.material, b.glvd);
        });
}

void render_gl_render_queue(Registry& r)
{
    auto& viewport = get_render_context_viewport(r);

    // Grab and bind fbo if set
    auto& fbo = viewport.fbo;
    if (fbo) {
        fbo->bind();
    }

    auto& cam = viewport.computed_camera;
    const auto P = cam.get_perspective();
    const auto V = cam.get_view();
    const auto cameraPos = cam.get_position();
    const auto screen_size = cam.get_window_size();

    Shader* last_shader = nullptr;
    std::unique_ptr<GLScope> shader_scope = nullptr; // RAII GL scope

    auto& queue = r.ctx().emplace<GLRenderQueue>();


    for (const auto& qitem : queue) {
        auto& shader = *qitem.shader;
        const auto& material = *qitem.material;
        const auto& glvd = *qitem.glvd;
        const auto entity = qitem.entity;
        const auto primitive = qitem.primitive;


        if (last_shader != &shader) {
            shader_scope = std::make_unique<GLScope>(); // Replace GL scope (pop and push)
            activate_shader(r, shader, get_render_context(r));
            last_shader = &shader;
        }


        GLScope gl;


        // Is the material set to query?
        GLenum query_type = material.int_values.count(RasterizerOptions::Query)
                                ? material.int_values.at(RasterizerOptions::Query)
                                : GL_NONE;
        GLuint query_id = 0;


        // Skip items that are not applicable to the material query
        if (query_type != GL_NONE) {
            if (!r.valid(qitem.entity)) continue;
            if (!r.all_of<GLQuery>(qitem.entity)) continue;
            const auto& q = r.get<GLQuery>(qitem.entity);
            if (q.type != query_type) continue;
            query_id = q.id;
        }


        // Current render layer
        int offset_layer = get_render_context(r).polygon_offset_layer;

        // If Pass option is set, set offset layer manually
        if (material.int_values.count(RasterizerOptions::Pass)) {
            offset_layer = material.int_values.at(RasterizerOptions::Pass);
        }

        // Offset depth in perspective matrix
        const auto offset_P = utils::render::offset_depth(P, offset_layer);
        const auto offset_PV = (offset_P * V).eval();
        const auto offset_PVinv = offset_PV.inverse().eval();

        // Assign global built ins (outer uniform buffer)
        shader["PV"] = offset_PV;
        shader["PVinv"] = offset_PVinv;

        shader["cameraPos"] = cameraPos;
        shader["screen_size"] = screen_size;
        shader["object_id"] = int(entity);

        if (r.all_of<Camera::ViewportTransform>(entity)) {
            auto obj_cam =
                viewport.computed_camera.transformed(r.get<Camera::ViewportTransform>(entity));
            gl(glViewport,
               int(obj_cam.get_window_origin().x()),
               int(obj_cam.get_window_origin().y()),
               int(obj_cam.get_window_width()),
               int(obj_cam.get_window_height()));

            shader["PV"] = obj_cam.get_PV();
            shader["PVinv"] = obj_cam.get_PV().inverse().eval();
            shader["screen_size"] = obj_cam.get_window_size();
        }

        if (r.all_of<Transform>(entity)) {
            auto& t = r.get<Transform>(entity);
            shader["M"] = t.global;
#ifdef LAGRANGE_UI_PRECISE_NORMAL_MAT
            const Eigen::Matrix4f Nmat = normal_matrix(t.global);
#else
            const Eigen::Matrix4f Nmat = t.global.matrix().inverse().transpose();
#endif

            shader["NMat"] = Nmat;

        } else {
            shader["M"] = Eigen::Affine3f::Identity();
            shader["NMat"] = Eigen::Affine3f::Identity();
        }

        shader["alpha_multiplier"] = 1.0f;

        for (auto& it : material.int_values) {
            // Set uniform if exists
            shader.uniform(it.first) = it.second;

            // Check for rasterizer options
            switch (it.first) {
            case RasterizerOptions::DepthTest:
                if (it.second == GL_FALSE)
                    gl(glDisable, GL_DEPTH_TEST);
                else
                    gl(glEnable, GL_DEPTH_TEST);
                break;
            case RasterizerOptions::CullFaceEnabled:
                if (it.second == GL_FALSE)
                    gl(glDisable, GL_CULL_FACE);
                else
                    gl(glEnable, GL_CULL_FACE);
                break;
            case RasterizerOptions::BlendEquation: gl(glBlendEquation, it.second); break;
            case RasterizerOptions::DrawBuffer:
#if defined(__EMSCRIPTEN__)
                // TODO WebGL: glDrawBuffer is not supported.
                // Consider using glDrawBuffers instead.
                log_error_once(r, "WebGL does not support glDrawBuffer.");

#else
                gl(glDrawBuffer, it.second);
#endif
                break;
            case RasterizerOptions::ReadBuffer: gl(glReadBuffer, it.second); break;
            case RasterizerOptions::CullFace: gl(glCullFace, it.second); break;
            case RasterizerOptions::DepthMask: gl(glDepthMask, it.second); break;
            case RasterizerOptions::ScissorTest:
                if (it.second == GL_FALSE)
                    gl(glDisable, GL_SCISSOR_TEST);
                else
                    gl(glEnable, GL_SCISSOR_TEST);
                break;
            case RasterizerOptions::ColorMask:
                gl(glColorMask, it.second, it.second, it.second, it.second);
                break;
            };
        }

        if (material.int_values.count(RasterizerOptions::ScissorX) &&
            material.int_values.count(RasterizerOptions::ScissorY) &&
            material.int_values.count(RasterizerOptions::ScissorWidth) &&
            material.int_values.count(RasterizerOptions::ScissortHeight)) {
            gl(glScissor,
               material.int_values.at(RasterizerOptions::ScissorX),
               material.int_values.at(RasterizerOptions::ScissorY),
               material.int_values.at(RasterizerOptions::ScissorWidth),
               material.int_values.at(RasterizerOptions::ScissortHeight));
        }


        // Check for comibnation of rasterizer options
        if (material.int_values.count(RasterizerOptions::BlendSrcRGB) &&
            material.int_values.count(RasterizerOptions::BlendDstRGB)) {
            const int srcAlpha = material.int_values.count(RasterizerOptions::BlendSrcAlpha)
                                     ? material.int_values.at(RasterizerOptions::BlendSrcAlpha)
                                     : GL_ONE;
            const int dstAlpha = material.int_values.count(RasterizerOptions::BlendDstAlpha)
                                     ? material.int_values.at(RasterizerOptions::BlendDstAlpha)
                                     : GL_ONE;

            gl(glBlendFuncSeparate,
               material.int_values.at(RasterizerOptions::BlendSrcRGB),
               material.int_values.at(RasterizerOptions::BlendDstRGB),
               srcAlpha,
               dstAlpha);
        }


        if (material.int_values.count(RasterizerOptions::PolygonMode)) {
#if defined(__EMSCRIPTEN__)
            log_error_once(r, "WebGL does not support glPolygonMode.");
#else
            const int mode = material.int_values.at(RasterizerOptions::PolygonMode);
            gl(glPolygonMode, GL_FRONT_AND_BACK, mode);
#endif
        }

        if (material.float_values.count(RasterizerOptions::PointSize)) {
#if defined(__EMSCRIPTEN__)
            log_error_once(r, "WebGL does not support glPointSize.");
#else
            gl(glPointSize, material.float_values.at(RasterizerOptions::PointSize));
#endif
        } else if (material.int_values.count(RasterizerOptions::PointSize)) {
#if defined(__EMSCRIPTEN__)
            log_error_once(r, "WebGL does not support glPointSize.");
#else
            gl(glPointSize, float(material.int_values.at(RasterizerOptions::PointSize)));
#endif
        }

        if (material.int_values.count(RasterizerOptions::DepthFunc)) {
            gl(glDepthFunc, material.int_values.at(RasterizerOptions::DepthFunc));
        }


        for (auto& it : material.mat4_values) {
            shader.uniform(it.first) = it.second;
        }

        for (auto& it : material.mat4_array_values) {
            shader.uniform(it.first).set_matrices(it.second.data(), int(it.second.size()));
        }

        for (auto& it : material.vec4_values) {
            shader.uniform(it.first) = it.second;
        }

        for (auto& it : material.vec4_array_values) {
            shader.uniform(it.first).set_vectors(it.second.data(), int(it.second.size()));
        }


        for (auto& it : material.texture_values) {
            const auto& tex_val = it.second;


            // Find the texture unit
            auto it_samplers = shader.sampler_indices().find(it.first);

            if (it_samplers == shader.sampler_indices().end()) {
                // Unused sampler
                continue;
            }


            const auto& name = shader.name(it.first);

            const bool sampler_exists = it_samplers != shader.sampler_indices().end();

            // If texture is set and sampler exists in shader
            if (tex_val.texture && sampler_exists) {
                // Bind the texture to predefined tex unit
                tex_val.texture->bind_to(GL_TEXTURE0 + it_samplers->second);
                shader.uniform(name + "_texture_bound") = true;
            } else if (sampler_exists) {
                shader.uniform(name + "_texture_bound") = false;
                const auto it_prop = shader.texture_properties().find(it.first);
                // Set default value if there is a texture property defined
                if (it_prop != shader.texture_properties().end()) {
                    const auto& prop = it_prop->second;
                    // Set default color if there's no texture
                    if (prop.value_dimension == 1) {
                        shader.uniform(name + "_default_value") = it.second.color.x();
                    } else if (prop.value_dimension == 2) {
                        shader.uniform(name + "_default_value") =
                            Eigen::Vector2f(it.second.color.x(), it.second.color.y());
                    } else if (prop.value_dimension == 3) {
                        shader.uniform(name + "_default_value") = Eigen::Vector3f(
                            it.second.color.x(),
                            it.second.color.y(),
                            it.second.color.z());
                    } else if (prop.value_dimension == 4) {
                        shader.uniform(name + "_default_value") = it.second.color;
                    }
                }
            } else {
                lagrange::logger().warn(
                    "There is no texture sampler for {} in the shader (Shader ID {})",
                    name,
                    material.shader_id());
            }
        }

        for (auto& it : material.float_values) {
            shader.uniform(it.first) = it.second;
        }

        for (auto& it : material.color_values) {
            auto it_prop = shader.color_properties().find(it.first);
            if (it_prop != shader.color_properties().end() && it_prop->second.is_attrib) {
                shader.attrib(it.first) = it.second;
            } else {
                shader.uniform(it.first) = it.second;
            }
        }


        if (query_type != GL_NONE) {
            glBeginQuery(query_type, query_id);
        }


        if (material.int_values.count(RasterizerOptions::Primitive)) {
            auto raster_primitive = material.int_values.at(RasterizerOptions::Primitive);
            render_vertex_data(glvd, raster_primitive, get_gl_primitive_size(raster_primitive));
        } else {
            render_vertex_data(glvd, get_gl_primitive(primitive), get_gl_primitive_size(primitive));
        }

        if (query_type != GL_NONE) {
            glEndQuery(query_type);
        }
    }


    // Collect query results at once
    for (const auto& qitem : queue) {
        const auto& material = *qitem.material;

        // Is the material set to query?
        GLenum query_type = material.int_values.count(RasterizerOptions::Query)
                                ? material.int_values.at(RasterizerOptions::Query)
                                : GL_NONE;
        GLuint query_id = 0;


        // Skip items that are not applicable to the material query
        if (query_type != GL_NONE) {
            if (!r.valid(qitem.entity)) continue;
            if (!r.all_of<GLQuery>(qitem.entity)) continue;
            const auto& q = r.get<GLQuery>(qitem.entity);
            if (q.type != query_type) continue;
            query_id = q.id;
        }


        if (query_type != GL_NONE) {
#if defined(__EMSCRIPTEN__)
            // Signed integer queries are not supported in WebGL. Use an unsigned integer and cast.
            GLuint result;
            glGetQueryObjectuiv(query_id, GL_QUERY_RESULT, &result);
            r.get<GLQuery>(qitem.entity).result = safe_cast<GLint>(result);
#else
            glGetQueryObjectiv(query_id, GL_QUERY_RESULT, &r.get<GLQuery>(qitem.entity).result);
#endif
        }
    }
}


void render_geometry(Registry& r)
{
    // Render context must be set
    assert(get_render_context(r).viewport != NullEntity);
    {
        get_brdflut(r); // Make sure its computed
    }

    meshrender_to_render_queue(r);
    sort_gl_render_queue(r);
    render_gl_render_queue(r);

    return;
}


struct PostProcessQuadVertexData
{
    GLMesh quad;
};

void render_post_process(Registry& r)
{
    // Render context must be set
    assert(get_render_context(r).viewport != NullEntity);


    auto& queue = r.ctx().emplace<GLRenderQueue>();
    queue.clear();

    auto viewport_entity = get_render_context(r).viewport;
    auto& viewport = get_render_context_viewport(r);

    // Cache a quad mesh
    r.ctx().emplace<PostProcessQuadVertexData>(PostProcessQuadVertexData{generate_quad_mesh_gpu()});
    auto& ppquad = r.ctx().get<PostProcessQuadVertexData>().quad;

    std::unordered_map<Material*, VertexData> attrib_bindings;

    for (auto& it : viewport.post_process_effects) {
        auto& mat = it.second;
        assert(mat);

        auto shader_res = get_shader(r, mat->shader_id());
        if (!shader_res) continue;

        auto& shader = *shader_res;

        attrib_bindings[mat.get()] = VertexData{};


        for (auto& tex : mat->texture_values) {
            if (tex.second.texture && tex.second.texture->get_params().type == GL_TEXTURE_2D) {
                tex.second.texture->bind();

                if (tex.second.texture->get_params().generate_mipmap) {
                    glGenerateMipmap(GL_TEXTURE_2D);
                }
            }
        }

        update_vertex_data(
            ppquad,
            shader,
            attrib_bindings[mat.get()],
            IndexingMode::FACE,
            entt::null);

        GLRenderQueueItem item;
        item.pass = 0;
        auto pass_it = mat->int_values.find(RasterizerOptions::Pass);
        if (pass_it != mat->int_values.end()) {
            item.pass = pass_it->second;
        }

        item.shader = &shader;
        item.material = mat.get();
        item.glvd = &attrib_bindings[mat.get()];
        item.primitive = PrimitiveType::TRIANGLES;
        item.entity = viewport_entity;


        queue.emplace_back(std::move(item));
    }


    render_gl_render_queue(r);
}

} // namespace ui
} // namespace lagrange

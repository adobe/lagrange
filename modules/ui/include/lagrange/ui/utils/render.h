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
#pragma once

#include <lagrange/ui/types/Camera.h>
#include <lagrange/ui/types/FrameBuffer.h>
#include <lagrange/ui/types/Shader.h>
#include <lagrange/ui/components/GLMesh.h>
#include <lagrange/ui/components/VertexData.h>
#include <tuple>


namespace lagrange {
namespace ui {

class Material;

namespace utils {
namespace render {

/// Adds a depth offset to the perspective `projection` matrix
/// 
/// `layer_index` specifies how many discrete offsets are applied
/// Returns Eigen::Matrix4f perspective matrix with depth offset
/// 
/// Based on http://www.terathon.com/gdc07_lengyel.pdf (slide 18)
Eigen::Matrix4f offset_depth(const Eigen::Projective3f& perspective, int layer_index);


///
/// Sets render pass default OpenGL state
///
/// Multisample: off
/// Blending: a_src, 1-a_src, 1, 1
/// Depth clamping: on
/// Depth func: less or equal
/// Seamless cube maps: on
void set_render_pass_defaults(GLScope& scope);


////
/// Computes a plane perpendicular to direction
///
/// Returns a pair of orthogonal directions, that together with direction form a orthogonal basis
std::pair<Eigen::Vector3f, Eigen::Vector3f> compute_perpendicular_plane(Eigen::Vector3f direction);


} // namespace render


} // namespace utils


//==============================================================
/*
    New rendering system pipeline utils
*/
//==============================================================
struct Transform;

/*
    Sets shader uniforms (PV, PVinv, M, NMat, screen_size) based on camera and optional transform
    Adjusts viewport glViewport if transform contains viewport transform
*/
void set_render_transforms(
    GLScope& scope,
    Shader& shader,
    const Camera& camera,
    const Transform* transform = nullptr);


void render_vertex_data(const VertexData& vd, GLenum primitive, GLsizei per_element_size);

GLenum get_gl_primitive(const PrimitiveType& p);

GLsizei get_gl_primitive_size(const PrimitiveType& p);
GLsizei get_gl_primitive_size(GLenum primitive_enum);


std::shared_ptr<VertexData> generate_cube_vertex_data(bool edges = false);
std::shared_ptr<VertexData> generate_quad_vertex_data();

GLMesh generate_quad_mesh_gpu();


void update_vao(VertexData& vertex_data);


entt::resource_handle<Shader> get_or_load_shader(
    entt::resource_cache<Shader>& cache,
    const std::string& generic_path,
    bool virtual_fs = false);


template <typename BufferComponent>
bool set_mesh_geometry_layout(
    const Registry& registry,
    const Entity mesh_geometry_entity,
    const int& location,
    VertexData& vertex_data)
{
    const auto* buffer_component = registry.try_get<BufferComponent>(mesh_geometry_entity);

    vertex_data.attribute_dimensions[location] = BufferComponent::dimension;

    if (buffer_component && buffer_component->buffer) {
        vertex_data.attribute_buffers[location] = buffer_component->buffer;
        return true;
    } else {
        vertex_data.attribute_buffers[location] = nullptr;
        return false;
    }
}

int get_gl_attribute_dimension(GLenum attrib_type);

/// @brief Assigns buffers from GLMesh to GLVertexData to Shader specified locations
/// @param glmesh collection of gpu buffers
/// @param shader
/// @param glvd
/// @param indexing
void update_vertex_data(
    const GLMesh& glmesh,
    const Shader& shader,
    VertexData& glvd,
    IndexingMode indexing,
    entt::id_type submesh_index
);

} // namespace ui
} // namespace lagrange

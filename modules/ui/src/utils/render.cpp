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
#include <lagrange/ui/types/Material.h>
#include <lagrange/ui/utils/render.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/strings.h>

// new
#include <lagrange/ui/components/Common.h>
#include <lagrange/ui/components/RenderContext.h>
#include <lagrange/ui/components/Transform.h>
#include <lagrange/ui/types/ShaderLoader.h>

namespace lagrange {
namespace ui {
namespace utils {
namespace render {

std::pair<Eigen::Vector3f, Eigen::Vector3f> compute_perpendicular_plane(Eigen::Vector3f direction)
{
    Eigen::Vector3f& u1 = direction;
    Eigen::Vector3f v2;

    if (std::abs(direction.x()) == 1.0f && direction.y() == 0.0f && direction.z() == 0.0f) {
        v2 = Eigen::Vector3f(0, 1, 0);
    } else {
        v2 = Eigen::Vector3f(1, 0, 0);
    }

    Eigen::Vector3f u2 = v2 - vector_projection(v2, u1);

    Eigen::Vector3f u3 = u2.cross(u1);

    return {u2.normalized(), u3.normalized()};
}


Eigen::Matrix4f offset_depth(const Eigen::Projective3f& perspective, int layer_index)
{
    const float offset_eps = 4.8e-7f;
    Eigen::Matrix4f offset_P = perspective.matrix();
    const float total_offset = layer_index * offset_eps;
    offset_P(2, 2) += total_offset;
    return offset_P;
}

void set_render_pass_defaults(GLScope& scope)
{
#if !defined(__EMSCRIPTEN__)
    // TODO WebGL: GL_DEPTH_CLAMP is not supported.
    scope(glEnable, GL_DEPTH_CLAMP);
#endif
    scope(glDepthFunc, GL_LEQUAL);

    scope(glEnable, GL_BLEND);
    scope(glBlendFuncSeparate, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
    scope(glBlendEquation, GL_FUNC_ADD);

#if !defined(__EMSCRIPTEN__)
    // TODO WebGL: GL_MULTISAMPLE is not supported.
    scope(glDisable, GL_MULTISAMPLE);

    // TODO WebGL: GL_TEXTURE_CUBE_MAP_SEAMLESS is not supported.
    scope(glEnable, GL_TEXTURE_CUBE_MAP_SEAMLESS);
#endif
}

} // namespace render
} // namespace utils


void set_render_transforms(GLScope& scope, Shader& shader, const Camera& camera, const Transform* t)
{
    if (t) {
        // Viewport transform
        auto object_cam = camera.transformed(t->viewport);
        scope(
            glViewport,
            int(object_cam.get_window_origin().x()),
            int(object_cam.get_window_origin().y()),
            int(object_cam.get_window_width()),
            int(object_cam.get_window_height()));

        shader["PV"] = object_cam.get_PV();
        shader["PVinv"] = object_cam.get_PV().inverse().eval(); // TODO more efficient
        shader["screen_size"] = object_cam.get_window_size();
        shader["M"] = t->global;
        shader["NMat"] = normal_matrix(t->global);
    } else {
        shader["PV"] = camera.get_PV();
        shader["PVinv"] = camera.get_PV().inverse().eval(); // TODO more efficient
        shader["screen_size"] = camera.get_window_size();
        shader["M"] = Eigen::Affine3f::Identity();
        shader["NMat"] = Eigen::Affine3f::Identity();
    }
}

void render_vertex_data(const VertexData& vd, GLenum primitive, GLsizei per_element_size)
{
    if (!vd.vao) return;

    glBindVertexArray(vd.vao->id);
    if (!vd.index_buffer) {
        // No index buffer, first attribute is driver
        auto& pos_buf = vd.attribute_buffers.front()->vbo();
        LA_GL(glDrawArrays(primitive, 0, pos_buf.count));
    } else {
        auto& indices = vd.index_buffer->vbo();
        LA_GL(glDrawElements(primitive, per_element_size * indices.count, indices.glType, 0));
    }
    glBindVertexArray(0);
}

GLenum get_gl_primitive(const PrimitiveType& p)
{
    switch (p) {
    case PrimitiveType::POINTS: return GL_POINTS;
    case PrimitiveType::LINES: return GL_LINES;
    case PrimitiveType::TRIANGLES: return GL_TRIANGLES;
    }
    return 0;
}

GLsizei get_gl_primitive_size(const PrimitiveType& p)
{
    switch (p) {
    case PrimitiveType::POINTS: return 1;
    case PrimitiveType::LINES: return 2;
    case PrimitiveType::TRIANGLES: return 3;
    }
    return 0;
}

GLsizei get_gl_primitive_size(GLenum primitive_enum)
{
    switch (primitive_enum) {
    case GL_POINTS: return 1;
    case GL_LINES: return 2;
    case GL_TRIANGLES: return 3;
    }
    return 0;
}

std::shared_ptr<VertexData> generate_cube_vertex_data(bool edges)
{
    Eigen::Matrix<float, Eigen::Dynamic, 3, Eigen::RowMajor> V(8, 3);
    V.row(0) = Eigen::Vector3f{-1, -1, 1};
    V.row(1) = Eigen::Vector3f{1, -1, 1};
    V.row(2) = Eigen::Vector3f{1, 1, 1};
    V.row(3) = Eigen::Vector3f{-1, 1, 1};
    V.row(4) = Eigen::Vector3f{-1, -1, -1};
    V.row(5) = Eigen::Vector3f{1, -1, -1};
    V.row(6) = Eigen::Vector3f{1, 1, -1};
    V.row(7) = Eigen::Vector3f{-1, 1, -1};


    if (edges) {
        Eigen::Matrix<float, Eigen::Dynamic, 3, Eigen::RowMajor> VE(12 * 2, 3);
        int i = 0;
        // Front face
        VE.row(i++) = V.row(0);
        VE.row(i++) = V.row(1);
        VE.row(i++) = V.row(1);
        VE.row(i++) = V.row(2);
        VE.row(i++) = V.row(2);
        VE.row(i++) = V.row(3);
        VE.row(i++) = V.row(3);
        VE.row(i++) = V.row(0);

        // Back face
        VE.row(i++) = V.row(4 + 0);
        VE.row(i++) = V.row(4 + 1);
        VE.row(i++) = V.row(4 + 1);
        VE.row(i++) = V.row(4 + 2);
        VE.row(i++) = V.row(4 + 2);
        VE.row(i++) = V.row(4 + 3);
        VE.row(i++) = V.row(4 + 3);
        VE.row(i++) = V.row(4 + 0);

        // Mid
        VE.row(i++) = V.row(0);
        VE.row(i++) = V.row(0 + 4);
        VE.row(i++) = V.row(1);
        VE.row(i++) = V.row(1 + 4);
        VE.row(i++) = V.row(2);
        VE.row(i++) = V.row(2 + 4);
        VE.row(i++) = V.row(3);
        VE.row(i++) = V.row(3 + 4);

        Eigen::Matrix<float, Eigen::Dynamic, 3, Eigen::RowMajor> VN(12 * 2, 3);
        for (auto j = 0; j < 12; j++) {
            auto N = ((VE.row(2 * j) + VE.row(2 * j + 1)) * 0.5f).normalized().eval();
            VN.row(2 * j) = N;
            VN.row(2 * j + 1) = N;
        }

        auto pos = std::make_shared<GPUBuffer>();
        auto normal = std::make_shared<GPUBuffer>();
        pos->vbo().upload(VE);
        normal->vbo().upload(VN);


        auto vd = std::make_shared<VertexData>();
        vd->attribute_buffers = {pos, normal};
        vd->attribute_dimensions = {3, 3};
        vd->index_buffer = nullptr;
        update_vao(*vd);
        return vd;

    } else {
        Eigen::Matrix<unsigned int, Eigen::Dynamic, 3, Eigen::RowMajor> F(12, 3);
        using Index = Eigen::Matrix<unsigned int, 1, 3, Eigen::RowMajor>;
        F.row(0) = Index{0, 1, 2};
        F.row(1) = Index{2, 3, 0};
        F.row(2) = Index{3, 2, 6};
        F.row(3) = Index{6, 7, 3};
        F.row(4) = Index{7, 6, 5};
        F.row(5) = Index{5, 4, 7};
        F.row(6) = Index{4, 0, 3};
        F.row(7) = Index{3, 7, 4};
        F.row(8) = Index{0, 5, 1};
        F.row(9) = Index{5, 0, 4};
        F.row(10) = Index{1, 5, 6};
        F.row(11) = Index{6, 2, 1};

        auto pos = std::make_shared<GPUBuffer>();
        auto indices = std::make_shared<GPUBuffer>(GL_ELEMENT_ARRAY_BUFFER);
        pos->vbo().upload(V);
        indices->vbo().upload(F);


        auto vd = std::make_shared<VertexData>();
        vd->attribute_buffers = {pos};
        vd->attribute_dimensions = {3};
        vd->index_buffer = indices;
        update_vao(*vd);

        return vd;
    }

    return nullptr;
}

std::shared_ptr<VertexData> generate_quad_vertex_data()
{
    Eigen::Matrix<float, Eigen::Dynamic, 3, Eigen::RowMajor> V(4, 3);
    Eigen::Matrix<float, Eigen::Dynamic, 2, Eigen::RowMajor> UV(4, 2);
    Eigen::Matrix<unsigned int, Eigen::Dynamic, 3, Eigen::RowMajor> F(2, 3);
    V.row(0) = Eigen::Vector3f{-1, -1, 0};
    V.row(1) = Eigen::Vector3f{1, -1, 0};
    V.row(2) = Eigen::Vector3f{1, 1, 0};
    V.row(3) = Eigen::Vector3f{-1, 1, 0};


    UV.row(0) = Eigen::Vector2f{0, 0};
    UV.row(1) = Eigen::Vector2f{1, 0};
    UV.row(2) = Eigen::Vector2f{1, 1};
    UV.row(3) = Eigen::Vector2f{0, 1};

    using Index = Eigen::Matrix<unsigned int, 1, 3, Eigen::RowMajor>;
    F.row(0) = Index{0, 1, 2};
    F.row(1) = Index{2, 3, 0};

    auto pos = std::make_shared<GPUBuffer>();
    auto uv = std::make_shared<GPUBuffer>();
    auto indices = std::make_shared<GPUBuffer>(GL_ELEMENT_ARRAY_BUFFER);

    pos->vbo().upload(std::move(V));
    uv->vbo().upload(std::move(UV));
    indices->vbo().upload(std::move(F));

    auto vd = std::make_shared<VertexData>();
    vd->attribute_buffers = {pos, nullptr, uv};
    vd->attribute_dimensions = {3, 3, 2};
    vd->index_buffer = indices;
    update_vao(*vd);
    return vd;
}

GLMesh generate_quad_mesh_gpu()
{
    Eigen::Matrix<float, Eigen::Dynamic, 3, Eigen::RowMajor> V(4, 3);
    Eigen::Matrix<float, Eigen::Dynamic, 2, Eigen::RowMajor> UV(4, 2);
    Eigen::Matrix<unsigned int, Eigen::Dynamic, 3, Eigen::RowMajor> F(2, 3);
    V.row(0) = Eigen::Vector3f{-1, -1, 0};
    V.row(1) = Eigen::Vector3f{1, -1, 0};
    V.row(2) = Eigen::Vector3f{1, 1, 0};
    V.row(3) = Eigen::Vector3f{-1, 1, 0};


    UV.row(0) = Eigen::Vector2f{0, 0};
    UV.row(1) = Eigen::Vector2f{1, 0};
    UV.row(2) = Eigen::Vector2f{1, 1};
    UV.row(3) = Eigen::Vector2f{0, 1};

    using Index = Eigen::Matrix<unsigned int, 1, 3, Eigen::RowMajor>;
    F.row(0) = Index{0, 1, 2};
    F.row(1) = Index{2, 3, 0};

    auto pos = std::make_shared<GPUBuffer>();
    auto uv = std::make_shared<GPUBuffer>();
    auto indices = std::make_shared<GPUBuffer>(GL_ELEMENT_ARRAY_BUFFER);

    pos->vbo().upload(std::move(V));
    uv->vbo().upload(std::move(UV));
    indices->vbo().upload(std::move(F));

    GLMesh m;
    m.attribute_buffers[DefaultShaderAtrribNames::Position] = pos;
    m.attribute_buffers[DefaultShaderAtrribNames::UV] = uv;
    m.index_buffers[DefaultShaderIndicesNames::TriangleIndices] = indices;

    return m;
}

void update_vao(VertexData& vd)
{
    /*
            VAO Bindings
        */

    bool any_buffer = false;
    for (auto& buf : vd.attribute_buffers) {
        any_buffer |= (buf != nullptr);
    }

    if (!any_buffer) {
        vd.vao = nullptr;
        return;
    }

    if (!vd.vao) {
        vd.vao = std::make_shared<VAO>();
        vd.vao->init();
        // TODO RAII
    }
    LA_GL(glBindVertexArray(vd.vao->id));

    for (size_t i = 0; i < vd.attribute_buffers.size(); i++) {
        auto& buffer = vd.attribute_buffers[i];

        if (!buffer) {
            LA_GL(glDisableVertexAttribArray(GLuint(i)));
            continue;
        }

        const auto& vbo = buffer->vbo();

        la_runtime_assert(
            vbo.target == GL_ARRAY_BUFFER,
            "Attribute buffer must be bound to GL_ARRAY_BUFFER");

        la_runtime_assert(
            vd.attribute_dimensions[i] > 0 && vd.attribute_dimensions[i] <= 4,
            "Attribute must be of dimension 1,2,3, or 4");

        LA_GL(glBindBuffer(vbo.target, vbo.id));
        LA_GL(glVertexAttribPointer(
            GLuint(i),
            vd.attribute_dimensions[i],
            vbo.glType,
            GL_FALSE,
            0,
            0));
        LA_GL(glEnableVertexAttribArray(GLuint(i)));

#ifdef LAGRANGE_TRACE_BUFFERS
        lagrange::logger().trace(
            "VAO: loc {}, target GL_ARRAY_BUFFER, id {}, dim {}, type {}",
            i,
            vbo.id,
            vd.attribute_dimensions[i],
            vbo.glType);
#endif
    }

    // Bind index array
    if (vd.index_buffer) {
        const auto& vbo = vd.index_buffer->vbo();

        la_runtime_assert(
            vbo.target == GL_ELEMENT_ARRAY_BUFFER,
            "Indexing buffer must be bound to GL_ELEMENT_ARRAY_BUFFER");
        LA_GL(glBindBuffer(vbo.target, vbo.id));
    }

    LA_GL(glBindVertexArray(0));
}

entt::resource_handle<Shader> get_or_load_shader(
    entt::resource_cache<Shader>& cache,
    const std::string& generic_path,
    bool virtual_fs /*= false*/)
{
    const auto shader_id = entt::hashed_string{generic_path.c_str()};

    if (!cache.contains(shader_id)) {
        return cache.load<ShaderLoader>(
            shader_id,
            generic_path,
            virtual_fs ? ShaderLoader::PathType::VIRTUAL : ShaderLoader::PathType::REAL);
    }
    return cache.handle(shader_id);
}


int get_gl_attribute_dimension(GLenum attrib_type)
{
    switch (attrib_type) {
    case GL_FLOAT_VEC2: return 2;
    case GL_FLOAT_VEC3: return 3;
    case GL_FLOAT_VEC4: return 4;
    case GL_FLOAT_MAT2: return 4;
    case GL_FLOAT_MAT3: return 9;
    case GL_FLOAT_MAT4: return 16;
    case GL_INT: return 1;
    case GL_BOOL: return 1;
    case GL_SHORT: return 1;
    case GL_FLOAT: return 1;
    case GL_DOUBLE: return 1;
    default: break;
    }
    return 0;
}

void update_vertex_data(
    const GLMesh& glmesh,
    const Shader& shader,
    VertexData& glvd,
    IndexingMode indexing,
    entt::id_type submesh_index)
{
    // todo multi mat
    auto& vd = glvd;

    // Go through shader inputs (attributes)
    auto& attribs = shader.attribs();
    for (const auto& it : attribs) {
        // If has a default name, assign automatically from GLMesh
        const auto buffer = glmesh.get_attribute_buffer(it.first);
        if (buffer) {
            vd.attribute_buffers[it.second.location] = buffer;
            vd.attribute_dimensions[it.second.location] =
                get_gl_attribute_dimension(it.second.type);
        }

        // Otherwise the layout has to be adjusted in custom render pass later
        //...
    }

    // Add indexing buffer
    if (submesh_index != entt::null) {
        vd.index_buffer = glmesh.get_submesh_buffer(submesh_index);
    } else if (indexing == IndexingMode::VERTEX) {
        vd.index_buffer = glmesh.get_index_buffer(DefaultShaderIndicesNames::VertexIndices);
    } else if (indexing == IndexingMode::EDGE) {
        vd.index_buffer = glmesh.get_index_buffer(DefaultShaderIndicesNames::EdgeIndices);
    } else if (indexing == IndexingMode::FACE) {
        // If empty -> uses implicit indexing instead
        vd.index_buffer = glmesh.get_index_buffer(DefaultShaderIndicesNames::TriangleIndices);
    } else {
        // Implicit corner indexing
        vd.index_buffer = nullptr;
    }

    update_vao(vd);
}

} // namespace ui
} // namespace lagrange

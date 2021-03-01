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
#include <lagrange/Logger.h>
#include <lagrange/ui/MeshBuffer.h>

#include <Eigen/Eigen>
#include <cassert>

namespace lagrange {
namespace ui {


// RAII wrapper
struct VertexBufferWrapper
{
    VertexBufferWrapper(const VertexBufferWrapper&) = delete;
    VertexBufferWrapper& operator=(const VertexBufferWrapper&) = delete;

    VertexBufferWrapper(VertexBufferWrapper&&) = default;
    VertexBufferWrapper& operator=(VertexBufferWrapper&&) = default;

    VertexBufferWrapper() { m_vbo.initialize(); }
    ~VertexBufferWrapper() { m_vbo.free(); }

    VertexBuffer& operator*() { return m_vbo; }

private:
    VertexBuffer m_vbo;
};


MeshBuffer::SubBufferID MeshBuffer::default_sub_id()
{
    return "__default";
}

MeshBuffer::MeshBuffer(bool homogeneous)
    : m_homogeneous(homogeneous)
{
    m_vao.init();
}

MeshBuffer::~MeshBuffer()
{
    m_vao.free();
}

VertexBuffer& MeshBuffer::get_sub_buffer(SubBufferType subbuffer_type, const SubBufferID& id)
{
    auto key = key_t{subbuffer_type, id};

    auto it = m_sub_buffers.find(key);
    if (it == m_sub_buffers.end()) {
        auto w = std::make_unique<VertexBufferWrapper>();
        auto& ref = **w;

        if (subbuffer_type == SubBufferType::INDICES) {
            ref.target = GL_ELEMENT_ARRAY_BUFFER;
        } else {
            ref.target = GL_ARRAY_BUFFER;
        }

        m_sub_buffers[key] = std::move(w);
        return ref;
    } else {
        return **(it->second);
    }
}

VertexBuffer* MeshBuffer::try_get_sub_buffer(
    SubBufferType subbuffer_type, const SubBufferID& id /*= MeshBuffer::default_sub_id()*/)
{
    const auto key = key_t{subbuffer_type, id};
    auto it = m_sub_buffers.find(key);
    if (it == m_sub_buffers.end()) {
        if (id == MeshBuffer::default_sub_id()) return nullptr;

        const auto key_default = key_t{subbuffer_type, MeshBuffer::default_sub_id()};
        it = m_sub_buffers.find(key_default);
        if (it == m_sub_buffers.end()) return nullptr;
    }

    return &(**(it->second));
}

bool MeshBuffer::render(Primitive primitive, const SubBufferSelection& selection)
{
    // Bind vertex atrribute array and pointers
    GL(glBindVertexArray(m_vao.id));

    const auto fail = []() {
        GL(glBindVertexArray(0));
        return false;
    };

    const auto get_selection = [&](SubBufferType type) {
        auto it = selection.find(type);
        if (it == selection.end()) return default_sub_id();
        return it->second;
    };

    const auto bind_indices = [](VertexBuffer* ibo) {
        assert(ibo->glType == GL_UNSIGNED_INT);
        GL(glBindBuffer(ibo->target, ibo->id));
    };

    const auto bind_buffer = [&](SubBufferType type) -> VertexBuffer* {
        auto sub = try_get_sub_buffer(type, get_selection(type));
        if (!sub) return nullptr;

        if (type == SubBufferType::INDICES) {
            assert(sub->glType == GL_UNSIGNED_INT);
        }

        GL(glBindBuffer(sub->target, sub->id));
        return sub;
    };

    const auto bind_attrib = [&](SubBufferType type, int elem_cnt) -> VertexBuffer* {
        auto sub = try_get_sub_buffer(type, get_selection(type));
        if (!sub) return nullptr;
        if (sub->size == 0) return nullptr;
        GL(glBindBuffer(sub->target, sub->id));
        GL(glEnableVertexAttribArray(GLuint(type)));
        GL(glVertexAttribPointer(GLuint(type), elem_cnt, sub->glType, GL_FALSE, 0, 0));
        return sub;
    };


    auto pos = bind_attrib(SubBufferType::POSITION, m_homogeneous ? 4 : 3);
    bind_attrib(SubBufferType::NORMAL, 3);
    if (!pos) {
        return fail();
    }
    bind_attrib(SubBufferType::UV, 2);
    bind_attrib(SubBufferType::COLOR, 4);
    bind_attrib(SubBufferType::TANGENT, 3);
    bind_attrib(SubBufferType::BITANGENT, 3);

    const auto gl_primitive = [](const Primitive& p) {
        switch (p) {
        case Primitive::POINTS: return GL_POINTS;
        case Primitive::LINES: return GL_LINES;
        case Primitive::TRIANGLES: return GL_TRIANGLES;
        }
        return 0;
    };

    const auto primitive_multipier = [&](const Primitive& p) -> GLsizei {
        switch (p) {
        case Primitive::POINTS: return 1;
        case Primitive::LINES: return 2;
        case Primitive::TRIANGLES: return 3;
        }
        return 0;
    };


    auto indices = bind_buffer(SubBufferType::INDICES);

    if (!indices) {
        // No index buffer
        GL(glDrawArrays(gl_primitive(primitive), 0, primitive_multipier(primitive) * pos->count));
    } else {
        bind_indices(indices);
        GL(glDrawElements(gl_primitive(primitive), indices->count, indices->glType, 0));
    }

    glBindVertexArray(0);
    return true;
}


size_t MeshBuffer::get_attribute_num() const
{
    auto it = m_sub_buffers.find({SubBufferType::POSITION, default_sub_id()});
    if (it == m_sub_buffers.end()) return 0;
    return (**it->second).count;
}

VertexBuffer* MeshBuffer::non_default_index_buffer(const std::string& sub_id) const
{
    if (sub_id == default_sub_id()) return nullptr;
    const auto key = key_t{SubBufferType::INDICES, sub_id};
    auto it = m_sub_buffers.find(key);
    if (it == m_sub_buffers.end()) return nullptr;
    return &(**(it->second));
}


MeshBuffer& MeshBuffer::quad()
{
    if (m_quad) return *m_quad;

    m_quad = std::make_unique<MeshBuffer>();

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

    m_quad->get_sub_buffer(MeshBuffer::SubBufferType::POSITION).upload(V);
    m_quad->get_sub_buffer(MeshBuffer::SubBufferType::UV).upload(UV);
    
    VertexBuffer::DataDescription desc;
    desc.count = F.size();
    desc.gl_type = GL_UNSIGNED_INT;
    desc.integral = true;
    m_quad->get_sub_buffer(MeshBuffer::SubBufferType::INDICES)
        .upload(F.data(), sizeof(unsigned int) * F.size(), desc); // as flat array, not matrix

    return *m_quad;
}

MeshBuffer& MeshBuffer::point()
{
    if (m_point) return *m_point;

    m_point = std::make_unique<MeshBuffer>();

    Eigen::Matrix<float, Eigen::Dynamic, 3, Eigen::RowMajor> V(1, 3);
    Eigen::Matrix<float, Eigen::Dynamic, 2, Eigen::RowMajor> UV(1, 2);

    V.row(0) = Eigen::Vector3f{0, 0, 0};
    UV.row(0) = Eigen::Vector2f{0, 0};

    m_point->get_sub_buffer(MeshBuffer::SubBufferType::POSITION).upload(V);
    m_point->get_sub_buffer(MeshBuffer::SubBufferType::UV).upload(UV);

    return *m_point;
}

MeshBuffer& MeshBuffer::infinite_plane()
{
    if (m_infinite_plane) return *m_infinite_plane;

    m_infinite_plane = std::make_unique<MeshBuffer>(true);

    Eigen::Matrix<float, Eigen::Dynamic, 4, Eigen::RowMajor> V(5, 4);
    Eigen::Matrix<float, Eigen::Dynamic, 2, Eigen::RowMajor> UV(4, 2);
    Eigen::Matrix<unsigned int, Eigen::Dynamic, 3, Eigen::RowMajor> F(4, 3);
    V.row(0) = Eigen::Vector4f{0, 0, 0, 1};
    V.row(1) = Eigen::Vector4f{1, 0, 0, 0};
    V.row(2) = Eigen::Vector4f{0, 0, 1, 0};
    V.row(3) = Eigen::Vector4f{-1, 0, 0, 0};
    V.row(4) = Eigen::Vector4f{0, 0, -1, 0};


    UV.row(0) = Eigen::Vector2f{0.5f, 0.5f};
    UV.row(1) = Eigen::Vector2f{1.0f, 0.5f};
    UV.row(2) = Eigen::Vector2f{0.5f, 1.0f};
    UV.row(3) = Eigen::Vector2f{0.0f, 0.5f};
    UV.row(3) = Eigen::Vector2f{0.5f, 0.0f};

    using Index = Eigen::Matrix<unsigned int, 1, 3, Eigen::RowMajor>;
    F.row(0) = Index{0, 1, 2};
    F.row(1) = Index{0, 2, 3};
    F.row(2) = Index{0, 3, 4};
    F.row(3) = Index{0, 4, 1};

    m_infinite_plane->get_sub_buffer(MeshBuffer::SubBufferType::POSITION).upload(V);
    m_infinite_plane->get_sub_buffer(MeshBuffer::SubBufferType::UV).upload(UV);


    VertexBuffer::DataDescription desc;
    desc.count = F.size();
    desc.gl_type = GL_UNSIGNED_INT;
    desc.integral = true;
    m_infinite_plane->get_sub_buffer(MeshBuffer::SubBufferType::INDICES)
        .upload(F.data(), sizeof(unsigned int) * F.size(), desc); // as flat array, not matrix

    return *m_infinite_plane;
}

MeshBuffer& MeshBuffer::cube(bool edges)
{
    auto& ptr = edges ? m_cube_edges : m_cube_triangles;
    if (ptr) return *ptr;

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
        ptr = std::make_unique<MeshBuffer>();

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

        ptr->get_sub_buffer(MeshBuffer::SubBufferType::POSITION).upload(VE);
        ptr->get_sub_buffer(MeshBuffer::SubBufferType::NORMAL).upload(VN);

    } else {
        ptr = std::make_unique<MeshBuffer>();


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

        ptr->get_sub_buffer(MeshBuffer::SubBufferType::POSITION).upload(V);

        VertexBuffer::DataDescription desc;
        desc.count = F.size();
        desc.gl_type = GL_UNSIGNED_INT;
        desc.integral = true;
        ptr->get_sub_buffer(MeshBuffer::SubBufferType::INDICES)
            .upload(F.data(), sizeof(unsigned int) * F.size(), desc); // as flat array, not matrix
    }

    return *ptr;
}

std::unique_ptr<MeshBuffer> MeshBuffer::m_quad = nullptr;
std::unique_ptr<MeshBuffer> MeshBuffer::m_cube_triangles = nullptr;
std::unique_ptr<MeshBuffer> MeshBuffer::m_cube_edges = nullptr;
std::unique_ptr<MeshBuffer> MeshBuffer::m_point = nullptr;
std::unique_ptr<MeshBuffer> MeshBuffer::m_infinite_plane = nullptr;


} // namespace ui
} // namespace lagrange

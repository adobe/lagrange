/*
 * Copyright 2019 Adobe. All rights reserved.
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

#include <lagrange/ui/api.h>
#include <lagrange/ui/types/GLContext.h>

#include <stdint.h>
#include <algorithm>
#include <array>
#include <vector>


namespace lagrange {
namespace ui {

template <typename T>
struct type_traits;
template <>
struct type_traits<uint32_t>
{
    enum { type = GL_UNSIGNED_INT, integral = 1 };
};
template <>
struct type_traits<int32_t>
{
    enum { type = GL_INT, integral = 1 };
};
template <>
struct type_traits<uint16_t>
{
    enum { type = GL_UNSIGNED_SHORT, integral = 1 };
};
template <>
struct type_traits<int16_t>
{
    enum { type = GL_SHORT, integral = 1 };
};
template <>
struct type_traits<uint8_t>
{
    enum { type = GL_UNSIGNED_BYTE, integral = 1 };
};
template <>
struct type_traits<int8_t>
{
    enum { type = GL_BYTE, integral = 1 };
};
template <>
struct type_traits<double>
{
    enum { type = GL_DOUBLE, integral = 0 };
};
template <>
struct type_traits<float>
{
    enum { type = GL_FLOAT, integral = 0 };
};

struct LA_UI_API VertexBuffer
{
    VertexBuffer(GLenum _target = GL_ARRAY_BUFFER);
    GLenum target; // most common: GL_ARRAY_BUFFER or GL_ELEMENT_ARRAY_BUFFER
    GLuint id;
    GLuint size;
    GLuint glType;
    bool is_integral;
    GLsizei count;

    template <typename Matrix>
    void upload(const Matrix& M);

    template <typename T>
    void upload(const std::vector<T>& arr, uint32_t component_count = 1);


    void initialize();

    struct DataDescription
    {
        GLsizei count;
        bool integral;
        GLenum gl_type;
    };

    void upload(const void* data, size_t byte_size, DataDescription& desc);

    void
    upload(GLuint byte_size, const uint8_t* data, GLsizei count, bool integral, GLenum gl_type);

    void download(GLuint size_, uint8_t* data) const;

    void free();
};


template <typename Matrix>
void VertexBuffer::upload(const Matrix& M)
{
    uint32_t component_size = sizeof(typename Matrix::Scalar);
    upload(
        static_cast<uint32_t>(M.size()) * component_size,
        reinterpret_cast<const uint8_t*>(M.data()),
        static_cast<GLsizei>(M.rows()),
        static_cast<bool>(type_traits<typename Matrix::Scalar>::integral),
        static_cast<GLuint>(type_traits<typename Matrix::Scalar>::type));
}


template <typename T>
void VertexBuffer::upload(const std::vector<T>& arr, uint32_t component_count)
{
    uint32_t component_size = sizeof(T);
    upload(
        static_cast<uint32_t>(arr.size()) * component_size,
        reinterpret_cast<const uint8_t*>(arr.data()),
        static_cast<GLsizei>(arr.size() / component_count),
        static_cast<bool>(type_traits<T>::integral),
        static_cast<GLuint>(type_traits<T>::type));
}


struct LA_UI_API VAO
{
    VAO()
        : id(0)
    {}
    GLuint id;

    void init();
    void free();
};


struct LA_UI_API GPUBuffer
{
    GPUBuffer(const GPUBuffer&) = delete;
    GPUBuffer& operator=(const GPUBuffer&) = delete;

    GPUBuffer(GPUBuffer&&) = default;
    GPUBuffer& operator=(GPUBuffer&&) = default;

    GPUBuffer(GLenum _target = GL_ARRAY_BUFFER)
        : m_vbo(_target)
    {
        m_vbo.initialize();
    }
    ~GPUBuffer() { m_vbo.free(); }

    VertexBuffer& operator*() { return m_vbo; }
    VertexBuffer* operator->() { return &m_vbo; }

    VertexBuffer& vbo() { return m_vbo; }

private:
    // todo combine these
    VertexBuffer m_vbo;
};


} // namespace ui
} // namespace lagrange

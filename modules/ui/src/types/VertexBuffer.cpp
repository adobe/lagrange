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
#include <lagrange/ui/types/VertexBuffer.h>

#include <cassert>

namespace lagrange {
namespace ui {

VertexBuffer::VertexBuffer(GLenum _target /*= GL_ARRAY_BUFFER*/)
    : target(_target)
    , id(0)
    , size(0)
    , is_integral(false)
    , count(0)
{}

void VertexBuffer::upload(
    GLuint size_ /* in bytes */,
    const uint8_t* data,
    GLsizei cnt,
    bool integral,
    GLenum gl_type)
{
    if (id == 0) {
        initialize();
    }
    size = size_;
    LA_GL(glBindBuffer(target, id));
#if defined(__EMSCRIPTEN__)
    //https://github.com/thismarvin/susurrus/issues/5
    LA_GL(glBufferData(target, size, data, GL_STATIC_DRAW));
#else
    LA_GL(glBufferData(target, size, data, GL_DYNAMIC_DRAW));
#endif


    this->is_integral = integral;
    this->count = cnt;
    this->glType = gl_type;
}

void VertexBuffer::upload(const void* data, size_t byte_size, DataDescription& desc)
{
    upload(
        GLuint(byte_size),
        reinterpret_cast<const uint8_t*>(data),
        desc.count,
        desc.integral,
        desc.gl_type);
}

void VertexBuffer::initialize()
{
    if (id != 0) assert(false);
    LA_GL(glGenBuffers(1, &id));
}

void VertexBuffer::download(GLuint size_, uint8_t* data) const
{
    LA_GL(glBindBuffer(target, id));
    LA_GL(glGetBufferSubData(target, 0, size_, data));
}

void VertexBuffer::free()
{
    if (id != 0) {
        LA_GL(glDeleteBuffers(1, &id));
    }
    id = 0;
}

void VAO::init()
{
    if (id != 0) assert(false);
    LA_GL(glGenVertexArrays(1, &id));
}

void VAO::free()
{
    if (id != 0) {
        LA_GL(glDeleteVertexArrays(1, &id));
    }
    id = 0;
}

} // namespace ui
} // namespace lagrange

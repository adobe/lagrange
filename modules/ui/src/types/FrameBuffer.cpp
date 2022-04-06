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
#include <lagrange/ui/types/FrameBuffer.h>
#include <lagrange/ui/types/Texture.h>
#include <lagrange/utils/assert.h>


namespace lagrange {
namespace ui {

FrameBuffer::FrameBuffer()
    : m_managed(true)
{
    LA_GL(glGenFramebuffers(1, &m_id));

    int max_colors;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &max_colors);
    m_color_attachments.resize(max_colors);
}

FrameBuffer::FrameBuffer(GLuint custom_id)
    : m_id(custom_id)
    , m_managed(false)
{
    int max_colors;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &max_colors);
    m_color_attachments.resize(max_colors);
}

FrameBuffer::~FrameBuffer()
{
    if (m_managed) {
        LA_GL(glDeleteFramebuffers(1, &m_id));
    }
}

void FrameBuffer::resize_attachments(int w, int h)
{
    // Resize bound textures
    if (m_depth_attachment) m_depth_attachment->resize(w, h);

    for (auto& color : m_color_attachments) {
        if (color) color->resize(w, h);
    }
}

void FrameBuffer::bind()
{
#ifdef DEBUG
    check_status();
#endif
    LA_GL(glBindFramebuffer(GL_FRAMEBUFFER, m_id));
}

void FrameBuffer::unbind()
{
    LA_GL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
}

void FrameBuffer::set_depth_attachement(
    std::shared_ptr<Texture> t,
    GLenum target /*= GL_TEXTURE_2D*/,
    int mipmap_level /*= 0*/)
{
    bind();
    m_depth_attachment = t;
    auto id = t ? t->get_id() : 0;
    t->bind();
    if (target == GL_TEXTURE_2D) {
        LA_GL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, target, id, mipmap_level));
    } else {
#if defined(__EMSCRIPTEN__)
        // TODO WebGL: glFramebufferTexture is not supported.
        logger().error("WebGL does not support glFramebufferTexture.");
#else
        LA_GL(glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, t->get_id(), mipmap_level));
#endif
    }
}

void FrameBuffer::set_color_attachement(
    unsigned int index,
    std::shared_ptr<Texture> t,
    GLenum target /*= GL_TEXTURE_2D*/,
    int mipmap_level /*= 0*/)
{
    if (index > m_color_attachments.size())
        la_runtime_assert(false, "Maximum color attachments reached");

#if defined(__EMSCRIPTEN__)
    if (t) {
        const auto& p = t->get_params();
        if (!Texture::is_internal_format_color_renderable(p.internal_format)) {
            logger().error(
                "Color attachment not renderable, texture format {},  elem type {}, internal "
                "format "
                "{}.",
                GLState::get_enum_string(p.format),
                GLState::get_enum_string(t->get_gl_element_type()),
                GLState::get_enum_string(p.internal_format));
        }
    }
#endif

    m_color_attachments[index] = t;
    bind();

    t->bind();
    auto id = t ? t->get_id() : 0;
    LA_GL(glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0 + index,
        target,
        id,
        mipmap_level));

    int cnt = 0;
    for (auto& att : m_color_attachments) {
        if (att)
            cnt++;
        else
            break;
    }

    static GLuint attachments[8] = {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2,
        GL_COLOR_ATTACHMENT3,
        GL_COLOR_ATTACHMENT4,
        GL_COLOR_ATTACHMENT5,
        GL_COLOR_ATTACHMENT6,
        GL_COLOR_ATTACHMENT7};

    glDrawBuffers(cnt, attachments);
}

GLuint FrameBuffer::get_id() const
{
    return m_id;
}

bool FrameBuffer::check_status() const
{
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    switch (status) {
    case GL_FRAMEBUFFER_COMPLETE: return true;
    case GL_FRAMEBUFFER_UNDEFINED: logger().error("GL_FRAMEBUFFER_UNDEFINED"); break;
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        logger().error("GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT ");
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        logger().error("GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT");
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
        logger().error("GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER ");
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
        logger().error("GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER ");
        break;
    case GL_FRAMEBUFFER_UNSUPPORTED: logger().error("GL_FRAMEBUFFER_UNSUPPORTED "); break;
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
        logger().error("GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE  ");
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
        logger().error("GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS ");
        break;
    }

    return false;
}

bool FrameBuffer::is_srgb() const
{
    if (m_color_attachments.size() == 0) return false;

    auto format = m_color_attachments.front()->get_params().internal_format;

    return format == GL_SRGB8 || format == GL_SRGB8_ALPHA8 || format == GL_COMPRESSED_SRGB ||
           format == GL_COMPRESSED_SRGB_ALPHA;
}

std::shared_ptr<Texture> FrameBuffer::get_color_attachement(int index) const
{
    return m_color_attachments[index];
}

std::shared_ptr<Texture> FrameBuffer::get_depth_attachment() const
{
    return m_depth_attachment;
}

int FrameBuffer::get_max_color_attachments()
{
    int max_colors;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &max_colors);
    return max_colors;
}

} // namespace ui
} // namespace lagrange

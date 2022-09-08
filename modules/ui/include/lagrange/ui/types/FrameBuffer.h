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

#include <memory>
#include <vector>

#include <lagrange/ui/types/GLContext.h>
#include <lagrange/ui/types/Texture.h>

namespace lagrange {
namespace ui {

/*
    FrameBuffer class
    Allows setting textures as color and depth attachements
    Shares ownership of the attached textures
*/
class FrameBuffer
{
public:
    /*
        Default constructor, the OpenGL FBO is owned by this object
    */
    FrameBuffer();

    /*
        Uses FBO with custom_id. Acts a wrapper (does not delete the OpenGL FBO)
        Use for default FBO or FBO allocated somewhere else.
    */
    FrameBuffer(GLuint custom_id);
    ~FrameBuffer();

    /*
        Resizes textures currently bound to this FBO
    */
    void resize_attachments(int w, int h);

    void bind();

    /*
        Unbinds FBO (binds to id=0)
    */
    static void unbind();

    void set_depth_attachement(
        std::shared_ptr<Texture> t,
        GLenum target = GL_TEXTURE_2D,
        int mipmap_level = 0);

    void set_color_attachement(
        unsigned int index,
        std::shared_ptr<Texture> t,
        GLenum target = GL_TEXTURE_2D,
        int mipmap_level = 0);

    GLuint get_id() const;

    bool check_status() const;
    bool is_srgb() const;

    std::shared_ptr<Texture> get_color_attachement(int index) const;
    std::shared_ptr<Texture> get_depth_attachment() const;

    static int get_max_color_attachments();

    bool has_color_attachment() const
    {
        return m_color_attachments.size() > 0 && (m_color_attachments.front() != nullptr);
    }

    bool has_depth_attachement() const { return m_depth_attachment != nullptr; }

private:
    GLuint m_id;
    std::vector<std::shared_ptr<Texture>> m_color_attachments;
    std::shared_ptr<Texture> m_depth_attachment;
    bool m_managed;
};

} // namespace ui
} // namespace lagrange

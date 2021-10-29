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
#include <lagrange/ui/types/Texture.h>
#include <lagrange/utils/la_assert.h>
#include <stb_image.h>
#include <stb_image_write.h>


#include <algorithm>
#include <cstring>

namespace lagrange {
namespace ui {

float sRGB_to_linear(float v)
{
    if (v <= 0.04045f)
        return v / 12.92f;
    else
        return std::pow((v + 0.055f) / 1.055f, 2.4f);
}

Eigen::Vector3f sRGB_to_linear(const Eigen::Vector3f& c)
{
    return Eigen::Vector3f(sRGB_to_linear(c.x()), sRGB_to_linear(c.y()), sRGB_to_linear(c.z()));
}

float linear_to_sRGB(float v)
{
    if (v <= 0.0031308f)
        return v * 12.92f;
    else
        return 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
}

Eigen::Vector3f linear_to_sRGB(const Eigen::Vector3f& c)
{
    return Eigen::Vector3f(linear_to_sRGB(c.x()), linear_to_sRGB(c.y()), linear_to_sRGB(c.z()));
}

Texture::Texture(const fs::path& file_path, const Params& params)
    : m_params(params)
{
    GL(glGenTextures(1, &m_id));

    // load
    if (params.type == GL_TEXTURE_2D) {
        load_data_from_image(file_path);
    } else {
        throw std::runtime_error("Only 2D texture loading is implemented");
    }

    set_common_params();
}

Texture::Texture(
    const void* image_data,
    size_t size,
    const Texture::Params& params /*= Texture::Params()*/)
    : m_params(params)
{
    GL(glGenTextures(1, &m_id));

    // load
    if (params.type == GL_TEXTURE_2D) {
        load_data_from_image(image_data, size);
    } else {
        throw std::runtime_error("Only 2D texture loading is implemented");
    }

    set_common_params();
}

Texture::Texture(const Params& params, int width, int height, int depth)
    : m_width(width)
    , m_height(height)
    , m_depth(depth)
    , m_params(params)
{
    GL(glGenTextures(1, &m_id));

    LA_ASSERT(params.internal_format != GL_NONE, "Must specify internal format (e.g. GL_RGBA)");

    if (width > 0) resize(width, height, depth, true);

    // If zero size, defer allocation to when size is known
}


Texture::~Texture()
{
    free();
}

void Texture::bind() const
{
    LA_ASSERT(
        ((m_params.internal_format == GL_SRGB8 || m_params.internal_format == GL_SRGB_ALPHA) &&
         m_params.sRGB) ||
            (m_params.internal_format != GL_SRGB8 && m_params.internal_format != GL_SRGB_ALPHA &&
             !m_params.sRGB) ||
            m_params.internal_format == GL_NONE,
        "Inconsistent SRGB format");

    GL(glBindTexture(m_params.type, m_id));
}

void Texture::bind_to(GLenum texture_unit) const
{
    GL(glActiveTexture(texture_unit));
    GL(glBindTexture(m_params.type, m_id));
}

void Texture::resize(int width, int height /*= 0*/, int depth /*= 0*/, bool force /*= false*/)
{
    if (!force && m_width == width && m_height == height && m_depth == depth) return;

    bind();

    m_width = width;
    m_height = height;
    m_depth = depth;

    if (m_params.type == GL_TEXTURE_2D) {
        LA_ASSERT(m_params.format != GL_NONE, "Must specify format (e.g. GL_RGBA)");
        GL(glTexImage2D(
            GL_TEXTURE_2D,
            0,
            m_params.internal_format,
            width,
            height,
            0,
            m_params.format,
            GL_FLOAT,
            nullptr));
    } else if (m_params.type == GL_TEXTURE_2D_MULTISAMPLE) {
        GL(glTexImage2DMultisample(
            GL_TEXTURE_2D_MULTISAMPLE,
            m_params.multisample_samples,
            m_params.internal_format,
            width,
            height,
            GL_TRUE));
    } else if (m_params.type == GL_TEXTURE_CUBE_MAP) {
        LA_ASSERT(m_params.format != GL_NONE, "Must specify format (e.g. GL_RGBA)");
        for (auto i = 0; i < 6; i++) {
            GL(glTexImage2D(
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                0,
                m_params.internal_format,
                width,
                height,
                0,
                m_params.format,
                GL_FLOAT,
                nullptr));
        }
    }

    set_common_params();
}

void Texture::upload(float* data)
{
    bind();
    GL(glTexImage2D(
        m_params.type,
        0,
        m_params.internal_format,
        m_width,
        m_height,
        0,
        m_params.format,
        GL_FLOAT,
        data));
    set_common_params();
}

void Texture::upload(unsigned char* data)
{
    bind();
    GL(glTexImage2D(
        m_params.type,
        0,
        m_params.internal_format,
        m_width,
        m_height,
        0,
        m_params.format,
        GL_UNSIGNED_BYTE,
        data));
    set_common_params();
}

void Texture::set_uv_transform(const Texture::Transform& uv_transform)
{
    m_params.uv_transform = uv_transform;
}

bool Texture::save_to(
    const fs::path& file_path,
    GLenum target /*= GL_TEXTURE_2D*/,
    int quality /*= 90*/)
{
    auto comp_cnt = [](GLenum gl_type) {
        if (gl_type == GL_RGB) return 3u;
        if (gl_type == GL_RED) return 1u;
        if (gl_type == GL_DEPTH_COMPONENT) return 1u;
        if (gl_type == GL_RGBA) return 4u;
        return 0u;
    };

    using elem_type = unsigned char;
    GLenum elem_type_gl = GL_UNSIGNED_BYTE;

    auto w = m_width;
    auto h = m_height;

    std::vector<elem_type> tex_data;
    const auto components = comp_cnt(get_params().format);
    const auto tex_elem_count = w * h * components;
    const auto row_stride = static_cast<int>(w * components * sizeof(elem_type));
    tex_data.resize(tex_elem_count);

    bind();

    if (w % 4 != 0) {
        if (w % 2 != 0) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
        } else {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
            glPixelStorei(GL_PACK_ALIGNMENT, 2);
        }
    }

    glGetTexImage(target, 0, get_params().format, elem_type_gl, tex_data.data());

    // stbi wants char (non wchar), so we convert to string to achieve that
    const std::string path_string = file_path.string();
    const auto ext = file_path.extension();

    int res = 0;
    stbi_flip_vertically_on_write(1);
    if (ext == ".bmp")
        res = stbi_write_bmp(path_string.c_str(), w, h, components, tex_data.data());
    else if (ext == ".png")
        res = stbi_write_png(path_string.c_str(), w, h, components, tex_data.data(), row_stride);
    else if (ext == ".jpg")
        res = stbi_write_jpg(path_string.c_str(), w, h, components, tex_data.data(), quality);
    else {
        LA_ASSERT(false, "Unsupported format " + ext.string());
    }
    return res != 0;
}

void Texture::set_common_params()
{
    bind();

    if (m_params.generate_mipmap) GL(glGenerateMipmap(m_params.type));

    if (!m_params.generate_mipmap && m_params.min_filter == GL_LINEAR_MIPMAP_LINEAR) {
        logger().warn(
            "Mipmap disabled for tex id {} but using min filter GL_LINEAR_MIPMAP_LINEAR",
            m_id);
    }

    if (m_params.type != GL_TEXTURE_2D_MULTISAMPLE) {
        GL(glTexParameteri(m_params.type, GL_TEXTURE_WRAP_S, m_params.wrap_s));
        GL(glTexParameteri(m_params.type, GL_TEXTURE_WRAP_T, m_params.wrap_t));
        GL(glTexParameteri(m_params.type, GL_TEXTURE_WRAP_R, m_params.wrap_r));
        if (m_params.wrap_s == GL_CLAMP_TO_BORDER || m_params.wrap_t == GL_CLAMP_TO_BORDER ||
            m_params.wrap_r == GL_CLAMP_TO_BORDER) {
            GL(glTexParameterfv(m_params.type, GL_TEXTURE_BORDER_COLOR, m_params.border_color));
        }
        GL(glTexParameteri(m_params.type, GL_TEXTURE_MAG_FILTER, m_params.mag_filter));
        GL(glTexParameteri(m_params.type, GL_TEXTURE_MIN_FILTER, m_params.min_filter));
    }
}

void Texture::free()
{
    if (m_id > 0) {
        GL(glDeleteTextures(1, &m_id));
        m_id = 0;
    }
}

void Texture::load_data_from_image(const fs::path& file_path)
{
    int width, height, n;
    stbi_set_flip_vertically_on_load(true);
    const std::string path_string = file_path.string();
    unsigned char* data = stbi_load(path_string.c_str(), &width, &height, &n, STBI_default);
    stbi_set_flip_vertically_on_load(false);
    if (data == nullptr || n == 0 || n > 4) {
        throw std::runtime_error("Cannot read texture " + file_path.string());
        return;
    }

    m_width = width;
    m_height = height;

    bind();

    m_params.format = GL_NONE;
    if (n == 1) {
        m_params.format = GL_RED;
        if (m_params.internal_format == GL_NONE) m_params.internal_format = GL_RED;
    } else if (n == 2) {
        m_params.format = GL_RG;
        if (m_params.internal_format == GL_NONE) m_params.internal_format = GL_RG;
    } else if (n == 3) {
        m_params.format = GL_RGB;
        if (m_params.internal_format == GL_NONE)
            m_params.internal_format = (m_params.sRGB) ? GL_SRGB : GL_RGB;
    } else if (n == 4) {
        m_params.format = GL_RGBA;
        if (m_params.internal_format == GL_NONE)
            m_params.internal_format = (m_params.sRGB) ? GL_SRGB_ALPHA : GL_RGBA;
    }

    GL(glTexImage2D(
        m_params.type,
        0,
        m_params.internal_format,
        m_width,
        m_height,
        0,
        m_params.format,
        GL_UNSIGNED_BYTE,
        data));

    stbi_image_free(data);
}

void Texture::load_data_from_image(const void* image_data, size_t size)
{
    int width, height, n;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load_from_memory(
        reinterpret_cast<const unsigned char*>(image_data),
        static_cast<int>(size),
        &width,
        &height,
        &n,
        STBI_default);
    stbi_set_flip_vertically_on_load(false);

    if (data == nullptr || n == 0 || n > 4) {
        throw std::runtime_error("Cannot read texture from memory");
        return;
    }

    m_width = width;
    m_height = height;

    bind();

    m_params.format = GL_NONE;
    if (n == 1) {
        m_params.format = GL_RED;
        if (m_params.internal_format == GL_NONE) m_params.internal_format = GL_RED;
    } else if (n == 2) {
        m_params.format = GL_RG;
        if (m_params.internal_format == GL_NONE) m_params.internal_format = GL_RG;
    } else if (n == 3) {
        m_params.format = GL_RGB;
        if (m_params.internal_format == GL_NONE)
            m_params.internal_format = (m_params.sRGB) ? GL_SRGB : GL_RGB;
    } else if (n == 4) {
        m_params.format = GL_RGBA;
        if (m_params.internal_format == GL_NONE)
            m_params.internal_format = (m_params.sRGB) ? GL_SRGB_ALPHA : GL_RGBA;
    }

    GL(glTexImage2D(
        m_params.type,
        0,
        m_params.internal_format,
        m_width,
        m_height,
        0,
        m_params.format,
        GL_UNSIGNED_BYTE,
        data));

    stbi_image_free(data);
}

Texture::Transform::Transform(
    Eigen::Vector2f scale_ /*= Eigen::Vector2f(1.0f)*/,
    Eigen::Vector2f offset_ /*= Eigen::Vector2f(0.0f)*/,
    float rotation_ /*= 0.0f */)
    : scale(scale_)
    , offset(offset_)
    , rotation(rotation_)
{}

Eigen::Matrix3f Texture::Transform::get_matrix() const
{
    Eigen::Matrix3f m = Eigen::Matrix3f::Identity();
    m(0, 0) = cosf(rotation);
    m(0, 1) = sinf(rotation);
    m(1, 0) = -sinf(rotation);
    m(1, 1) = cosf(rotation);
    m(2, 0) = offset.x();
    m(2, 1) = offset.y();

    Eigen::Matrix3f s = Eigen::Matrix3f::Identity();
    s(0, 0) = scale.x();
    s(1, 1) = scale.y();

    return m * s;
}

} // namespace ui
} // namespace lagrange

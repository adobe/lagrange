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
#include <lagrange/utils/assert.h>
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
    , m_gl_elem_type(GL_NONE)
{
    LA_GL(glGenTextures(1, &m_id));

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
    , m_gl_elem_type(GL_NONE)
{
    LA_GL(glGenTextures(1, &m_id));

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
    , m_gl_elem_type(GL_NONE)
{
    LA_GL(glGenTextures(1, &m_id));

    la_runtime_assert(
        params.internal_format != GL_NONE,
        "Must specify internal format (e.g. GL_RGBA8)");

    if (width > 0) resize(width, height, depth, true);

    // If zero size, defer allocation to when size is known
}


Texture::~Texture()
{
    free();
}

void Texture::bind() const
{
    la_runtime_assert(
        ((m_params.internal_format == GL_SRGB8 || m_params.internal_format == GL_SRGB8_ALPHA8) &&
         m_params.sRGB) ||
            (m_params.internal_format != GL_SRGB8 && m_params.internal_format != GL_SRGB8_ALPHA8 &&
             !m_params.sRGB) ||
            m_params.internal_format == GL_NONE,
        "Inconsistent SRGB format");

    LA_GL(glBindTexture(m_params.type, m_id));
}

void Texture::bind_to(GLenum texture_unit) const
{
    LA_GL(glActiveTexture(texture_unit));
    LA_GL(glBindTexture(m_params.type, m_id));
}

GLenum get_type_from_internal_format(GLenum internal_format)
{
    // The type passed to glTexImage2D needs to match the internal format.
    // See https://www.khronos.org/registry/OpenGL-Refpages/es3.0/html/glTexImage2D.xhtml.
    switch (internal_format) {
    case GL_R8_SNORM:
    case GL_R8I:
    case GL_RG8_SNORM:
    case GL_RG8I:
    case GL_RGB8_SNORM:
    case GL_RGB8I:
    case GL_RGBA8_SNORM:
    case GL_RGBA8I: return GL_BYTE;
    case GL_R32F:
    case GL_RG32F:
    case GL_RGB32F:
    case GL_RGBA32F:
    case GL_DEPTH_COMPONENT32F:
    case GL_RGB16F: // could also be GL_HALF_FLOAT
    case GL_RGBA16F: // could also be GL_HALF_FLOAT
    case GL_R16F: // could also be GL_HALF_FLOAT
    case GL_RG16F: // could also be GL_HALF_FLOAT
    case GL_R11F_G11F_B10F: // could also be GL_UNSIGNED_INT_10F_11F_11F_REV or GL_HALF_FLOAT
    case GL_RGB9_E5: // could also be GL_UNSIGNED_INT_5_9_9_9_REV or GL_HALF_FLOAT
        return GL_FLOAT;
    case GL_DEPTH32F_STENCIL8: return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
    case GL_DEPTH24_STENCIL8: return GL_UNSIGNED_INT_24_8;
    case GL_R32I:
    case GL_RG32I:
    case GL_RGB32I:
    case GL_RGBA32I: return GL_INT;
    case GL_R16I:
    case GL_RG16I:
    case GL_RGB16I:
    case GL_RGBA16I: return GL_SHORT;
    case GL_R32UI:
    case GL_RG32UI:
    case GL_RGB32UI:
    case GL_RGBA32UI:
    case GL_DEPTH_COMPONENT24: return GL_UNSIGNED_INT;
    case GL_RGB10_A2:
    case GL_RGB10_A2UI: return GL_UNSIGNED_INT_2_10_10_10_REV;
    case GL_R16UI:
    case GL_RG16UI:
    case GL_RGB16UI:
    case GL_RGBA16UI:
    case GL_DEPTH_COMPONENT16: return GL_UNSIGNED_SHORT;
    default: return GL_UNSIGNED_BYTE;
    }
}

void Texture::resize(int width, int height /*= 0*/, int depth /*= 0*/, bool force /*= false*/)
{
    if (!force && m_width == width && m_height == height && m_depth == depth) return;

    bind();

    m_width = width;
    m_height = height;
    m_depth = depth;

    m_gl_elem_type = get_type_from_internal_format(m_params.internal_format);

    if (m_params.type == GL_TEXTURE_2D) {
        la_runtime_assert(m_params.format != GL_NONE, "Must specify format (e.g. GL_RGBA)");
        LA_GL(glTexImage2D(
            GL_TEXTURE_2D,
            0,
            m_params.internal_format,
            width,
            height,
            0,
            m_params.format,
            m_gl_elem_type,
            nullptr));
    } else if (m_params.type == GL_TEXTURE_2D_MULTISAMPLE) {
#if defined(__EMSCRIPTEN__)
        // TODO WebGL: glTexImage2DMultisample is not supported.
        logger().error("WebGL does not support glTexImage2DMultisample.");
#else
        LA_GL(glTexImage2DMultisample(
            GL_TEXTURE_2D_MULTISAMPLE,
            m_params.multisample_samples,
            m_params.internal_format,
            width,
            height,
            GL_TRUE));
#endif
    } else if (m_params.type == GL_TEXTURE_CUBE_MAP) {
        la_runtime_assert(m_params.format != GL_NONE, "Must specify format (e.g. GL_RGBA)");
        for (auto i = 0; i < 6; i++) {
            LA_GL(glTexImage2D(
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                0,
                m_params.internal_format,
                width,
                height,
                0,
                m_params.format,
                m_gl_elem_type,
                nullptr));
        }
    }

    set_common_params();
}

void Texture::upload(float* data)
{
    bind();
    m_gl_elem_type = GL_FLOAT;

    if (m_width % 4 != 0) {
        if (m_width % 2 != 0) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
        } else {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
            glPixelStorei(GL_PACK_ALIGNMENT, 2);
        }
    }

    LA_GL(glTexImage2D(
        m_params.type,
        0,
        m_params.internal_format,
        m_width,
        m_height,
        0,
        m_params.format,
        m_gl_elem_type,
        data));
    set_common_params();
}

void Texture::upload(unsigned char* data)
{
    bind();

    m_gl_elem_type = GL_UNSIGNED_BYTE;
    if (m_width % 4 != 0) {
        if (m_width % 2 != 0) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
        } else {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
            glPixelStorei(GL_PACK_ALIGNMENT, 2);
        }
    }

    LA_GL(glTexImage2D(
        m_params.type,
        0,
        m_params.internal_format,
        m_width,
        m_height,
        0,
        m_params.format,
        m_gl_elem_type,
        data));
    set_common_params();
}

void Texture::set_uv_transform(const Texture::Transform& uv_transform)
{
    m_params.uv_transform = uv_transform;
}

std::optional<Texture::DownloadResult> Texture::download(GLenum target, int mip_level)
{
    Texture::DownloadResult result;

    using elem_type = unsigned char;
    auto comp_cnt = [](GLenum gl_type) {
        if (gl_type == GL_RGB) return 3u;
        if (gl_type == GL_RED) return 1u;
        if (gl_type == GL_DEPTH_COMPONENT) return 1u;
        if (gl_type == GL_RGBA) return 4u;
        return 0u;
    };


    GLenum elem_type_gl = GL_UNSIGNED_BYTE;

    if (m_width == 0 || m_height == 0) {
        lagrange::logger().error("Texture::download: zero width or height");
        return {};
    }

    const int levels = static_cast<int>(std::log2(std::max(m_width, m_height)));
    if (mip_level >= levels) {
        lagrange::logger().error(
            "Texture::download: mip_level {} exceeds number of mip levels {}",
            mip_level,
            levels);
        return {};
    }

    auto w = m_width >> mip_level;
    auto h = m_height >> mip_level;


    const auto components = comp_cnt(get_params().format);
    const auto tex_elem_count = w * h * components;
    const auto row_stride = static_cast<int>(w * components * sizeof(elem_type));
    result.data.resize(tex_elem_count);

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

#if defined(__EMSCRIPTEN__)
    // TODO WebGL: glGetTexImage is not supported.
    // Consider attaching a texture to a framebuffer and using glReadPixels to get the data.
    // See https://16892.net/?qa=944852/&show=944853.
    logger().error("WebGL does not support glGetTexImage.");
    (void)target;
    (void)elem_type_gl;
    return {};
#else
    glGetTexImage(target, mip_level, get_params().format, elem_type_gl, result.data.data());
#endif


    result.w = w;
    result.h = h;
    result.components = components;
    result.row_stride = row_stride;

    return result;
}

bool Texture::save_to(
    const fs::path& file_path,
    GLenum target /*= GL_TEXTURE_2D*/,
    int quality /*= 90*/,
    int mip_level)
{
    auto result = download(target, mip_level);
    if (!result) return false;

    // stbi wants char (non wchar), so we convert to string to achieve that
    const std::string path_string = file_path.string();
    const auto ext = file_path.extension();

    int res = 0;
    stbi_flip_vertically_on_write(1);
    if (ext == ".bmp")
        res = stbi_write_bmp(
            path_string.c_str(),
            result->w,
            result->h,
            result->components,
            result->data.data());
    else if (ext == ".png")
        res = stbi_write_png(
            path_string.c_str(),
            result->w,
            result->h,
            result->components,
            result->data.data(),
            result->row_stride);
    else if (ext == ".jpg")
        res = stbi_write_jpg(
            path_string.c_str(),
            result->w,
            result->h,
            result->components,
            result->data.data(),
            quality);
    else {
        la_runtime_assert(false, "Unsupported format " + ext.string());
    }
    return res != 0;
}

bool Texture::is_internal_format_color_renderable(GLenum internal_format)
{
#if defined(__EMSCRIPTEN__)
    // If EXT_color_buffer_float is enabled, other formats are color-renderable
    //https://github.com/mrdoob/three.js/issues/15493

    //https: //www.khronos.org/registry/OpenGL/specs/es/3.0/es_spec_3.0.pdf
    // Table 3.13 and 3.3
    return (
        internal_format == GL_RED || internal_format == GL_RG || internal_format == GL_RGB ||
        internal_format == GL_RGBA || internal_format == GL_DEPTH_COMPONENT ||
        internal_format == GL_DEPTH_STENCIL || internal_format == GL_R8 ||
        internal_format == GL_RG8 || internal_format == GL_RGB8 || internal_format == GL_RGB565 ||
        internal_format == GL_RGBA4 || internal_format == GL_RGB5_A1 ||
        internal_format == GL_RGBA8 || internal_format == GL_RGB10_A2 ||
        internal_format == GL_SRGB8_ALPHA8);
#endif

    return true;
}

void Texture::set_common_params()
{
    bind();

    if (m_params.generate_mipmap) {
        if (!is_internal_format_color_renderable(m_params.internal_format)) {
            logger().error("Texture format not color renderable");
            logger().error(
                "generating mipmap for format {},  elem type {}, interal format {},",
                GLState::get_enum_string(m_params.format),
                GLState::get_enum_string(m_params.internal_format),
                GLState::get_enum_string(m_gl_elem_type));
        } else {
            LA_GL(glGenerateMipmap(m_params.type));
        }
    }

    if (!m_params.generate_mipmap && m_params.min_filter == GL_LINEAR_MIPMAP_LINEAR) {
        logger().warn(
            "Mipmap disabled for tex id {} but using min filter GL_LINEAR_MIPMAP_LINEAR",
            m_id);
    }

    if (m_params.type != GL_TEXTURE_2D_MULTISAMPLE) {
        LA_GL(glTexParameteri(m_params.type, GL_TEXTURE_WRAP_S, m_params.wrap_s));
        LA_GL(glTexParameteri(m_params.type, GL_TEXTURE_WRAP_T, m_params.wrap_t));
        LA_GL(glTexParameteri(m_params.type, GL_TEXTURE_WRAP_R, m_params.wrap_r));
        if (m_params.wrap_s == GL_CLAMP_TO_BORDER || m_params.wrap_t == GL_CLAMP_TO_BORDER ||
            m_params.wrap_r == GL_CLAMP_TO_BORDER) {
#if defined(__EMSCRIPTEN__)
            // TODO WebGL: GL_TEXTURE_BORDER_COLOR is not supported.
            logger().error("WebGL does not support GL_TEXTURE_BORDER_COLOR.");
#else
            LA_GL(glTexParameterfv(m_params.type, GL_TEXTURE_BORDER_COLOR, m_params.border_color));
#endif
        }
        LA_GL(glTexParameteri(m_params.type, GL_TEXTURE_MAG_FILTER, m_params.mag_filter));
        LA_GL(glTexParameteri(m_params.type, GL_TEXTURE_MIN_FILTER, m_params.min_filter));
    }
}

void Texture::free()
{
    if (m_id > 0) {
        LA_GL(glDeleteTextures(1, &m_id));
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
        if (m_params.internal_format == GL_NONE) m_params.internal_format = GL_R8;
    } else if (n == 2) {
        m_params.format = GL_RG;
        if (m_params.internal_format == GL_NONE) m_params.internal_format = GL_RG8;
    } else if (n == 3) {
        m_params.format = GL_RGB;
        if (m_params.internal_format == GL_NONE)
            m_params.internal_format = (m_params.sRGB) ? GL_SRGB8 : GL_RGB8;
    } else if (n == 4) {
        m_params.format = GL_RGBA;
        if (m_params.internal_format == GL_NONE)
            m_params.internal_format = (m_params.sRGB) ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    }

    m_gl_elem_type = GL_UNSIGNED_BYTE;
    LA_GL(glTexImage2D(
        m_params.type,
        0,
        m_params.internal_format,
        m_width,
        m_height,
        0,
        m_params.format,
        m_gl_elem_type,
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
        if (m_params.internal_format == GL_NONE) m_params.internal_format = GL_R8;
    } else if (n == 2) {
        m_params.format = GL_RG;
        if (m_params.internal_format == GL_NONE) m_params.internal_format = GL_RG8;
    } else if (n == 3) {
        m_params.format = GL_RGB;
        if (m_params.internal_format == GL_NONE)
            m_params.internal_format = (m_params.sRGB) ? GL_SRGB8 : GL_RGB8;
    } else if (n == 4) {
        m_params.format = GL_RGBA;
        if (m_params.internal_format == GL_NONE)
            m_params.internal_format = (m_params.sRGB) ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    }

    m_gl_elem_type = GL_UNSIGNED_BYTE;

    LA_GL(glTexImage2D(
        m_params.type,
        0,
        m_params.internal_format,
        m_width,
        m_height,
        0,
        m_params.format,
        m_gl_elem_type,
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

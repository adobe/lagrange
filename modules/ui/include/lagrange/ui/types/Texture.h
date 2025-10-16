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

#include <lagrange/fs/filesystem.h>
#include <lagrange/ui/api.h>
#include <lagrange/ui/types/GLContext.h>
#include <lagrange/ui/utils/math.h>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>


namespace lagrange {
namespace ui {

class LA_UI_API Texture
{
public:
    struct Transform
    {
        Eigen::Vector2f scale;
        Eigen::Vector2f offset;
        float rotation;
        Transform(
            Eigen::Vector2f scale_ = Eigen::Vector2f::Constant(1.0f),
            Eigen::Vector2f offset_ = Eigen::Vector2f::Constant(0.0f),
            float rotation_ = 0.0f);
        Eigen::Matrix3f get_matrix() const;
    };

    struct Params
    {
        GLenum type;
        // Set if overriding default 8 bit color
        GLenum internal_format;
        GLenum format; // Set if creating empty texture
        GLenum mag_filter;
        GLenum min_filter;
        GLenum wrap_s;
        GLenum wrap_t;
        GLenum wrap_r;
        float border_color[4]; // Only when clamp to border
        bool generate_mipmap;
        bool sRGB;
        float gamma;
        int multisample_samples; // Used for GL_TEXTURE_2D_MULTISAMPLE
        Transform uv_transform;

        // Default
        Params()
            : type(GL_TEXTURE_2D)
            , internal_format(GL_NONE)
            , format(GL_NONE)
            , mag_filter(GL_LINEAR)
            , min_filter(GL_LINEAR_MIPMAP_LINEAR)
            , wrap_s(GL_REPEAT)
            , wrap_t(GL_REPEAT)
            , wrap_r(GL_REPEAT)
            , border_color{0, 0, 0, 0}
            , generate_mipmap(true)
            , sRGB(false)
            , gamma(1.0f)
            , multisample_samples(4)
        {}

        // Shorthands
        static Params multisampled_rgba16f(int num_samples = 4)
        {
            Params p;
            p.type = GL_TEXTURE_2D_MULTISAMPLE;
            p.internal_format = GL_RGBA16F;
            p.generate_mipmap = false;
            p.min_filter = GL_LINEAR;
            p.multisample_samples = num_samples;
            return p;
        }


        static Params multisampled_rgba16f_depth(int num_samples = 4)
        {
            Params p = multisampled_rgba16f(num_samples);
            p.internal_format = GL_DEPTH_COMPONENT24;
            return p;
        }

        static Params rgb16f()
        {
            Texture::Params p;
            p.type = GL_TEXTURE_2D;
            p.format = GL_RGB;
            p.internal_format = GL_RGB16F;
            p.wrap_s = GL_CLAMP_TO_EDGE;
            p.wrap_t = GL_CLAMP_TO_EDGE;
            p.mag_filter = GL_LINEAR;
            p.min_filter = GL_LINEAR;
            p.generate_mipmap = false;
            return p;
        }


        static Params rgba16f()
        {
            Params p = rgb16f();
            p.format = GL_RGBA;
            p.internal_format = GL_RGBA16F;
            return p;
        }

        static Params red8()
        {
            Texture::Params p;
            p.type = GL_TEXTURE_2D;
            p.format = GL_RED;
            p.internal_format = GL_RED;
            p.wrap_s = GL_CLAMP_TO_EDGE;
            p.wrap_t = GL_CLAMP_TO_EDGE;
            p.mag_filter = GL_LINEAR;
            p.min_filter = GL_LINEAR;
            p.generate_mipmap = false;
            return p;
        }

        static Params rgb()
        {
            Texture::Params p;
            p.type = GL_TEXTURE_2D;
            p.format = GL_RGB;
            p.internal_format = GL_RGB;
            p.wrap_s = GL_CLAMP_TO_EDGE;
            p.wrap_t = GL_CLAMP_TO_EDGE;
            p.mag_filter = GL_NEAREST;
            p.min_filter = GL_NEAREST;
            p.generate_mipmap = false;
            return p;
        }

        static Params rgba()
        {
            Params p = rgb();
            p.format = GL_RGBA;
            p.internal_format = GL_RGBA;
            return p;
        }

        static Params depth()
        {
            Params p = rgb();
            p.format = GL_DEPTH_COMPONENT;
            p.internal_format = GL_DEPTH_COMPONENT24;
            return p;
        }
    };

    Texture(const fs::path& file_path, const Texture::Params& params = Texture::Params()); // throws
    Texture(
        const void* image_data,
        size_t size,
        const Texture::Params& params = Texture::Params()); // throws

    // Create empty texture
    Texture(const Params& params, int width = 1, int height = 1, int depth = 0);

    ~Texture();

    void bind() const;

    void bind_to(GLenum texture_unit) const;

    GLuint get_id() const { return m_id; }

    const Params& get_params() const { return m_params; }

    // Resizes texture, destroys previous data
    void resize(int width, int height = 0, int depth = 0, bool force = false);

    int get_width() const { return m_width; }
    int get_height() const { return m_height; }

    void upload(float* data);
    void upload(unsigned char* data);

    void set_uv_transform(const Texture::Transform& uv_transform);

    struct DownloadResult
    {
        std::vector<unsigned char> data;
        int w, h, components;
        int row_stride;
    };

    std::optional<Texture::DownloadResult> download(
        GLenum target = GL_TEXTURE_2D,
        int mip_level = 0);

    bool save_to(
        const fs::path& file_path,
        GLenum opengl_target = GL_TEXTURE_2D,
        int quality = 90,
        int mip_level = 0);


    static bool is_internal_format_color_renderable(GLenum internal_format);

    // Get element type used to allocate the texture memory (e.g. GL_UNSIGNED_BYTE, GL_FLOAT, ...)
    GLenum get_gl_element_type() const { return m_gl_elem_type; }

private:
    void set_common_params();
    void free();
    void load_data_from_image(const fs::path& file_path);
    void load_data_from_image(const void* image_data, size_t size);

    int m_width;
    int m_height;
    int m_depth;
    GLuint m_id;

    Params m_params;
    GLenum m_gl_elem_type;
};

} // namespace ui
} // namespace lagrange

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
#include <lagrange/ui/IBL.h>

#include <fstream>
#include <string>

#include <lagrange/Logger.h>
#include <lagrange/fs/file_utils.h>
#include <lagrange/utils/utils.h>

#include <lagrange/ui/MeshBuffer.h>
#include <lagrange/ui/RenderResourceBuilder.h>
#include <lagrange/ui/default_resources.h>
#include <lagrange/ui/utils/math.h>


namespace lagrange {
namespace ui {

Emitter::Type IBL::get_type() const
{
    return Emitter::Type::IBL;
}

//http://www.hdrlabs.com/sibl/formatspecs.html
IBL::IBL(const fs::path& file_path)
    : Emitter()
    , m_file_path(file_path)
{
    if (file_path.extension() == ".ibl") {
        m_background_rect = get_bg_texture_ibl_file(file_path);
    } else {
        Texture::Params p;
        p.sRGB = true;
        m_background_rect = Resource<Texture>::create(file_path, p);
    }

    if (!m_background_rect) throw std::runtime_error("Failed to load IBL background texture");

    generate_textures(*m_background_rect);
}

IBL::IBL(const std::string& name, Resource<Texture> bg_texture)
    : m_background_rect(bg_texture)
    , m_name(name)
    , m_file_path("")
{
    if (!m_background_rect) throw std::runtime_error("Null IBL background texture");

    generate_textures(*m_background_rect);
}

bool IBL::save_to(const fs::path& file_path)
{
    auto base_file = fs::path(file_path).replace_extension().string();
    std::string ext = ".jpg";
    bool result = true;

    result |= m_background_rect->save_to(base_file + "_bg_rect" + ext, GL_TEXTURE_2D);

    result |= m_background->save_to(base_file + "_bg_pos_x" + ext, GL_TEXTURE_CUBE_MAP_POSITIVE_X);
    result |= m_background->save_to(base_file + "_bg_pos_y" + ext, GL_TEXTURE_CUBE_MAP_POSITIVE_Y);
    result |= m_background->save_to(base_file + "_bg_pos_z" + ext, GL_TEXTURE_CUBE_MAP_POSITIVE_Z);
    result |= m_background->save_to(base_file + "_bg_neg_x" + ext, GL_TEXTURE_CUBE_MAP_NEGATIVE_X);
    result |= m_background->save_to(base_file + "_bg_neg_y" + ext, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y);
    result |= m_background->save_to(base_file + "_bg_neg_z" + ext, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);

    result |=
        m_specular->save_to(base_file + "_specular_pos_x" + ext, GL_TEXTURE_CUBE_MAP_POSITIVE_X);
    result |=
        m_specular->save_to(base_file + "_specular_pos_y" + ext, GL_TEXTURE_CUBE_MAP_POSITIVE_Y);
    result |=
        m_specular->save_to(base_file + "_specular_pos_z" + ext, GL_TEXTURE_CUBE_MAP_POSITIVE_Z);
    result |=
        m_specular->save_to(base_file + "_specular_neg_x" + ext, GL_TEXTURE_CUBE_MAP_NEGATIVE_X);
    result |=
        m_specular->save_to(base_file + "_specular_neg_y" + ext, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y);
    result |=
        m_specular->save_to(base_file + "_specular_neg_z" + ext, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);

    result |=
        m_diffuse->save_to(base_file + "_diffuse_pos_x" + ext, GL_TEXTURE_CUBE_MAP_POSITIVE_X);
    result |=
        m_diffuse->save_to(base_file + "_diffuse_pos_y" + ext, GL_TEXTURE_CUBE_MAP_POSITIVE_Y);
    result |=
        m_diffuse->save_to(base_file + "_diffuse_pos_z" + ext, GL_TEXTURE_CUBE_MAP_POSITIVE_Z);
    result |=
        m_diffuse->save_to(base_file + "_diffuse_neg_x" + ext, GL_TEXTURE_CUBE_MAP_NEGATIVE_X);
    result |=
        m_diffuse->save_to(base_file + "_diffuse_neg_y" + ext, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y);
    result |=
        m_diffuse->save_to(base_file + "_diffuse_neg_z" + ext, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);

    return result;
}

void IBL::generate_textures(const Texture& input_tex)
{
    GLScope gl;

    gl(glEnable, GL_TEXTURE_CUBE_MAP_SEAMLESS);


    Texture::Params cube_map_params;
    cube_map_params.type = GL_TEXTURE_CUBE_MAP;
    cube_map_params.format = GL_RGB;
    cube_map_params.internal_format = GL_SRGB;
    cube_map_params.wrap_s = GL_CLAMP_TO_EDGE;
    cube_map_params.wrap_t = GL_CLAMP_TO_EDGE;
    cube_map_params.mag_filter = GL_LINEAR;
    cube_map_params.min_filter = GL_LINEAR;
    cube_map_params.generate_mipmap = false;

    const Eigen::Projective3f P = perspective(lagrange::to_radians(90.0f), 1.0f, 0.1f, 10.0f);
    const Eigen::Vector3f origin = Eigen::Vector3f::Zero();
    const Eigen::Matrix4f V[6] = {
        look_at(origin, Eigen::Vector3f(1, 0, 0), Eigen::Vector3f(0, -1, 0)),
        look_at(origin, Eigen::Vector3f(-1, 0, 0), Eigen::Vector3f(0, -1, 0)),
        look_at(origin, Eigen::Vector3f(0, 1, 0), Eigen::Vector3f(0, 0, 1)),
        look_at(origin, Eigen::Vector3f(0, -1, 0), Eigen::Vector3f(0, 0, -1)),
        look_at(origin, Eigen::Vector3f(0, 0, 1), Eigen::Vector3f(0, -1, 0)),
        look_at(origin, Eigen::Vector3f(0, 0, -1), Eigen::Vector3f(0, -1, 0))};
    const Eigen::Matrix4f M = Eigen::Matrix4f::Identity();

    auto shader_to_cube = Resource<Shader>::create(
        ShaderResourceParams{ShaderResourceParams::Tag::VIRTUAL_PATH, "cubemap/to_cube.shader"});
    auto shader_convolve = Resource<Shader>::create(
        ShaderResourceParams{ShaderResourceParams::Tag::VIRTUAL_PATH, "cubemap/convolve.shader"});
    auto shader_specular = Resource<Shader>::create(
        ShaderResourceParams{ShaderResourceParams::Tag::VIRTUAL_PATH, "cubemap/specular.shader"});

    auto tex_cube = Resource<Texture>::create(cube_map_params);
    auto tex_diffuse = Resource<Texture>::create(cube_map_params);

    auto specular_tex_params = cube_map_params;
    specular_tex_params.min_filter = GL_LINEAR_MIPMAP_LINEAR;
    specular_tex_params.generate_mipmap = true;
    specular_tex_params.internal_format = GL_RGB16F;
    auto tex_specular = Resource<Texture>::create(specular_tex_params);

    auto& cube = MeshBuffer::cube();
    FrameBuffer fbo;

    // Rectangular to cubemap
    {
        const int size = input_tex.get_width() / 4;

        fbo.bind();
        shader_to_cube->bind();
        // shader_to_cube.bind();
        tex_cube->resize(size, size);

        gl(glDisable, GL_MULTISAMPLE);
        gl(glDisable, GL_DEPTH_TEST);
        gl(glDisable, GL_BLEND);
        gl(glDisable, GL_CULL_FACE);
        gl(glViewport, 0, 0, size, size);

        (*shader_to_cube)["M"] = M;
        (*shader_to_cube)["NMat"] = M;

        for (auto i = 0; i < 6; i++) {
            fbo.set_color_attachement(0, tex_cube, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i);
            fbo.resize_attachments(size, size);
            gl(glClearColor, 0.0f, 0.0f, 0.0f, 0.0f);
            gl(glClear, GL_COLOR_BUFFER_BIT);

            if (fbo.is_srgb()) {
                gl(glEnable, GL_FRAMEBUFFER_SRGB);
            }

            input_tex.bind_to(GL_TEXTURE0 + 0);
            (*shader_to_cube)["texRectangular"] = 0;

            Eigen::Matrix4f PV = (P * V[i]);
            (*shader_to_cube)["PV"] = PV;

            cube.render(MeshBuffer::Primitive::TRIANGLES);
        }
    }

    // Diffuse (convolution)
    {
        const int size = 512;
        fbo.bind();
        tex_diffuse->resize(size, size);
        shader_convolve->bind();

        (*shader_convolve)["M"] = M;
        (*shader_convolve)["NMat"] = M;

        for (auto i = 0; i < 6; i++) {
            fbo.set_color_attachement(0, tex_diffuse, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i);
            gl(glViewport, 0, 0, size, size);
            gl(glClearColor, 0.0f, 0.0f, 0.0f, 0.0f);
            gl(glClear, GL_COLOR_BUFFER_BIT);

            Eigen::Matrix4f PV = (P * V[i]);
            (*shader_convolve)["PV"] = PV;

            tex_cube->bind_to(GL_TEXTURE0);
            (*shader_convolve)["texCube"] = 0;

            cube.render(MeshBuffer::Primitive::TRIANGLES);
        }
    }

    // Specular
    {
        const int size = 512;

        fbo.bind();
        tex_specular->resize(size, size);
        fbo.check_status();

        shader_specular->bind();

        (*shader_specular)["M"] = M;
        (*shader_specular)["NMat"] = M;

        const int levels = static_cast<int>(std::log2(size));
        int mip_size = size;

        for (auto mip_level = 0; mip_level < levels; mip_level++) {
            gl(glViewport, 0, 0, mip_size, mip_size);

            // Roughness increases with mip level
            (*shader_specular)["roughness"] = 0.0f + mip_level / float(levels - 1);
            for (auto i = 0; i < 6; i++) {
                fbo.set_color_attachement(
                    0,
                    tex_specular,
                    GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                    mip_level);
                gl(glClearColor, 0.0f, 0.0f, 0.0f, 0.0f);
                gl(glClear, GL_COLOR_BUFFER_BIT);

                Eigen::Matrix4f PV = (P * V[i]);
                (*shader_specular)["PV"] = PV;

                tex_cube->bind_to(GL_TEXTURE0);
                (*shader_specular)["texCube"] = 0;

                cube.render(MeshBuffer::Primitive::TRIANGLES);
            }

            mip_size = mip_size / 2;
        }
    }


    // Collect textures
    m_background = tex_cube;
    m_specular = tex_specular;
    m_diffuse = tex_diffuse;
}

Resource<Texture> IBL::get_bg_texture_ibl_file(const fs::path& ibl_file_path)
{
    fs::ifstream f(ibl_file_path);

    if (!f.good()) throw std::runtime_error("Couldn't open IBL path: " + ibl_file_path.string());

    std::string buffer;
    buffer.reserve(256);

    auto nextToken = [](std::istream& s, std::string& buf) {
        buf.clear();

        bool has_quote = false;
        char c = 0;
        while (true) {
            if (!s.good()) {
                return;
            }

            c = s.get();
            if (c == '"') {
                if (!has_quote) {
                    has_quote = true;
                } else {
                    return;
                }
            }

            if (!has_quote && (c == ' ' || c == '\n')) {
                return;
            }

            if (c != '"') {
                buf.push_back(c);
            }
        }
    };

    std::unordered_map<std::string, std::string> values = {
        {"Name", ""},
        {"BGfile", ""},
        {"BGmap", ""},
        {"EVfile", ""},
        {"EVmap", ""},
        {"EVgamma", ""},
        {"REFfile", ""},
        {"REFmap", ""},
        {"REFgamma", ""}};

    while (f.good()) {
        nextToken(f, buffer);

        for (auto& it : values) {
            if (it.second != "") continue;
            if (buffer == it.first) {
                nextToken(f, buffer); // equals
                nextToken(f, buffer); // value
                it.second = buffer;
            }
        }
    }

    for (const auto& it : values) {
        if (it.second == "") {
            throw std::runtime_error((it.first + " not found in ibl file").c_str());
        }
    }

    m_name = values["Name"];

    if (values["BGmap"] != "1") return Resource<Texture>();

    Texture::Params p;
    p.sRGB = true;
    return Resource<Texture>::create(
        (ibl_file_path.parent_path() / fs::path(values["BGfile"])).string(),
        p);
}

} // namespace ui
} // namespace lagrange

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
#include <lagrange/ui/types/Texture.h>
#include <lagrange/ui/utils/ibl.h>

#include <lagrange/ui/Entity.h>
#include <lagrange/ui/types/ShaderLoader.h>
#include <lagrange/ui/utils/render.h>
#include <lagrange/utils/utils.h>

namespace lagrange {
namespace ui {

namespace {

std::shared_ptr<Texture> get_bg_texture_ibl_file(const fs::path& ibl_file_path)
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


    if (values["BGmap"] != "1") return nullptr;

    Texture::Params p;
    p.sRGB = true;

    auto bg_file_path = ibl_file_path.parent_path() / fs::path(values["BGfile"]);

    return std::make_shared<Texture>(bg_file_path, p);
}

} // namespace


IBL generate_ibl(const fs::path& path, size_t resolution)
{
    std::shared_ptr<Texture> bgrecttex;

    if (path.extension() == ".ibl") {
        bgrecttex = get_bg_texture_ibl_file(path);
    } else {
        Texture::Params p;
        p.sRGB = true;
        bgrecttex = std::make_shared<Texture>(path, p);
    }

    if (!bgrecttex) throw std::runtime_error("Failed to load IBL background texture");

    return generate_ibl(bgrecttex, resolution);
}


IBL generate_ibl(const std::shared_ptr<Texture>& background_texture, size_t resolution)
{
    IBL iblc;
    iblc.background_rect = background_texture;


    Registry r;

    /*
        Register shaders
    */
    auto shader_to_cube = register_shader(r, []() {
        ShaderDefinition d;
        d.path = "cubemap/to_cube.shader";
        d.path_type = ShaderLoader::PathType::VIRTUAL;
        return d;
    }());

    auto shader_convolve = register_shader(r, []() {
        ShaderDefinition d;
        d.path = "cubemap/convolve.shader";
        d.path_type = ShaderLoader::PathType::VIRTUAL;
        return d;
    }());

    auto shader_specular = register_shader(r, []() {
        ShaderDefinition d;
        d.path = "cubemap/specular.shader";
        d.path_type = ShaderLoader::PathType::VIRTUAL;
        return d;
    }());

    Texture::Params cube_map_params;
    cube_map_params.type = GL_TEXTURE_CUBE_MAP;
    cube_map_params.format = GL_RGB;
    cube_map_params.internal_format = GL_SRGB;
    cube_map_params.wrap_s = GL_CLAMP_TO_EDGE;
    cube_map_params.wrap_t = GL_CLAMP_TO_EDGE;
    cube_map_params.mag_filter = GL_LINEAR;
    cube_map_params.min_filter = GL_LINEAR;
    cube_map_params.generate_mipmap = false;

    auto specular_tex_params = cube_map_params;
    specular_tex_params.min_filter = GL_LINEAR_MIPMAP_LINEAR;
    specular_tex_params.generate_mipmap = true;
    specular_tex_params.internal_format = GL_RGB16F;

    auto tex_cube = std::make_shared<Texture>(cube_map_params);
    auto tex_diffuse = std::make_shared<Texture>(cube_map_params);
    auto tex_specular = std::make_shared<Texture>(specular_tex_params);

    auto vd = generate_cube_vertex_data();


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

    GLScope gl;

    gl(glEnable, GL_TEXTURE_CUBE_MAP_SEAMLESS);

    FrameBuffer fbo;

    // Rectangular to cubemap
    {
        const int size = int(resolution);
        auto& shader = *get_shader(r, shader_to_cube);


        fbo.bind();
        shader.bind();
        tex_cube->resize(size, size);

        gl(glDisable, GL_MULTISAMPLE);
        gl(glDisable, GL_DEPTH_TEST);
        gl(glDisable, GL_BLEND);
        gl(glDisable, GL_CULL_FACE);
        gl(glViewport, 0, 0, size, size);

        shader["M"] = M;
        shader["NMat"] = M;

        for (auto i = 0; i < 6; i++) {
            // tmp


            fbo.set_color_attachement(
                0,
                tex_cube,
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i);
            fbo.resize_attachments(size, size);
            gl(glClearColor, 0.0f, 0.0f, 0.0f, 0.0f);
            gl(glClear, GL_COLOR_BUFFER_BIT);

            if (fbo.is_srgb()) {
                gl(glEnable, GL_FRAMEBUFFER_SRGB);
            }

            background_texture->bind_to(GL_TEXTURE0 + 0);
            shader["texRectangular"] = 0;

            Eigen::Matrix4f PV = (P * V[i]);
            shader["PV"] = PV;

            render_vertex_data(*vd, GL_TRIANGLES, 3);
        }
    }

    // Diffuse (convolution)
    {
        const int size = int(resolution) / 2;
        auto& shader = *get_shader(r, shader_convolve);

        fbo.bind();
        tex_diffuse->resize(size, size);
        shader.bind();

        shader["M"] = M;
        shader["NMat"] = M;

        for (auto i = 0; i < 6; i++) {
            fbo.set_color_attachement(
                0,
                tex_diffuse,
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i);
            gl(glViewport, 0, 0, size, size);
            gl(glClearColor, 0.0f, 0.0f, 0.0f, 0.0f);
            gl(glClear, GL_COLOR_BUFFER_BIT);

            Eigen::Matrix4f PV = (P * V[i]);
            shader["PV"] = PV;

            tex_cube->bind_to(GL_TEXTURE0);
            shader["texCube"] = 0;
            render_vertex_data(*vd, GL_TRIANGLES, 3);
        }
    }

    // Specular
    {
        const int size = int(resolution) / 2;
        auto& shader = *get_shader(r, shader_specular);

        fbo.bind();
        tex_specular->resize(size, size);
        fbo.check_status();

        shader.bind();

        shader["M"] = M;
        shader["NMat"] = M;

        const int levels = static_cast<int>(std::log2(size));
        int mip_size = size;

        for (auto mip_level = 0; mip_level < levels; mip_level++) {
            gl(glViewport, 0, 0, mip_size, mip_size);

            // Roughness increases with mip level
            shader["roughness"] = 0.0f + mip_level / float(levels - 1);
            for (auto i = 0; i < 6; i++) {
                fbo.set_color_attachement(
                    0,
                    tex_specular,
                    GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                    mip_level);
                gl(glClearColor, 0.0f, 0.0f, 0.0f, 0.0f);
                gl(glClear, GL_COLOR_BUFFER_BIT);

                Eigen::Matrix4f PV = (P * V[i]);
                shader["PV"] = PV;

                tex_cube->bind_to(GL_TEXTURE0);
                shader["texCube"] = 0;

                render_vertex_data(*vd, GL_TRIANGLES, 3);
            }

            mip_size = mip_size / 2;
        }
    }


    iblc.background = tex_cube;
    iblc.diffuse = tex_diffuse;
    iblc.specular = tex_specular;

    return iblc;
}

Entity get_ibl_entity(const Registry& registry)
{
    auto ibls = registry.view<const IBL>();
    if (ibls.size() == 0) return NullEntity;
    return ibls.front();
}

const IBL* get_ibl(const Registry& registry)
{
    auto e = get_ibl_entity(registry);
    if (!registry.valid(e)) return nullptr;
    return &registry.get<IBL>(e);
}

IBL* get_ibl(Registry& registry)
{
    auto e = get_ibl_entity(registry);
    if (!registry.valid(e)) return nullptr;
    return &registry.get<IBL>(e);
}

} // namespace ui
} // namespace lagrange

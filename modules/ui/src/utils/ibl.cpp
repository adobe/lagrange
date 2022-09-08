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
#include <lagrange/ui/types/Texture.h>
#include <lagrange/ui/utils/ibl.h>

#include <lagrange/ui/Entity.h>
#include <lagrange/ui/types/ShaderLoader.h>
#include <lagrange/ui/utils/render.h>
#include <lagrange/ui/utils/treenode.h>
#include <lagrange/utils/strings.h>
#include <lagrange/utils/utils.h>


#include <fstream>

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
    p.sRGB = false;

    auto bg_file_path = ibl_file_path.parent_path() / fs::path(values["BGfile"]);

    return std::make_shared<Texture>(bg_file_path, p);
}


Texture::Params tex_params_cube()
{
    Texture::Params cube_map_params;
    cube_map_params.type = GL_TEXTURE_CUBE_MAP;
    cube_map_params.format = GL_RGB;
    cube_map_params.internal_format = GL_RGB;
    cube_map_params.wrap_s = GL_CLAMP_TO_EDGE;
    cube_map_params.wrap_t = GL_CLAMP_TO_EDGE;
    cube_map_params.mag_filter = GL_LINEAR;
    cube_map_params.min_filter = GL_LINEAR_MIPMAP_LINEAR;
    cube_map_params.generate_mipmap = true;
    return cube_map_params;
}
Texture::Params tex_params_diffuse()
{
    auto diffuse_tex_params = tex_params_cube();
    diffuse_tex_params.min_filter = GL_LINEAR;
    return diffuse_tex_params;
}
Texture::Params tex_params_specular()
{
    auto specular_tex_params = tex_params_cube();
    specular_tex_params.min_filter = GL_LINEAR_MIPMAP_LINEAR;
    specular_tex_params.generate_mipmap = true;
    specular_tex_params.internal_format = GL_RGB16F;
    return specular_tex_params;
}

} // namespace


IBL generate_ibl(const fs::path& path, size_t resolution)
{
    std::shared_ptr<Texture> bgrecttex;

    if (path.extension() == ".ibl") {
        bgrecttex = get_bg_texture_ibl_file(path);
    } else {
        Texture::Params p;
        p.sRGB = false;
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


    auto tex_cube = std::make_shared<Texture>(tex_params_cube());
    auto tex_diffuse = std::make_shared<Texture>(tex_params_specular());
    auto tex_specular = std::make_shared<Texture>(tex_params_diffuse());

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

#if !defined(__EMSCRIPTEN__)
    // TODO WebGL: GL_TEXTURE_CUBE_MAP_SEAMLESS is not supported.
    gl(glEnable, GL_TEXTURE_CUBE_MAP_SEAMLESS);
#endif

    FrameBuffer fbo;

    // Rectangular to cubemap
    {
        const int size = int(resolution);
        auto& shader = *get_shader(r, shader_to_cube);


        fbo.bind();
        shader.bind();
        tex_cube->resize(size, size);

#if !defined(__EMSCRIPTEN__)
        // TODO WebGL: GL_MULTISAMPLE is not supported.
        gl(glDisable, GL_MULTISAMPLE);
#endif
        gl(glDisable, GL_DEPTH_TEST);
        gl(glDisable, GL_BLEND);
        gl(glDisable, GL_CULL_FACE);
        gl(glViewport, 0, 0, size, size);

        shader["M"] = M;
        shader["NMat"] = M;

        for (auto i = 0; i < 6; i++) {
            // tmp


            fbo.set_color_attachement(0, tex_cube, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i);
            fbo.resize_attachments(size, size);
            gl(glClearColor, 0.0f, 0.0f, 0.0f, 0.0f);
            gl(glClear, GL_COLOR_BUFFER_BIT);

            if (fbo.is_srgb()) {
#if !defined(__EMSCRIPTEN__)
                // TODO WebGL: GL_FRAMEBUFFER_SRGB is not supported.
                gl(glEnable, GL_FRAMEBUFFER_SRGB);
#endif
            }

            background_texture->bind_to(GL_TEXTURE0 + 0);
            shader["texRectangular"] = 0;

            Eigen::Matrix4f PV = (P * V[i]);
            shader["PV"] = PV;

            render_vertex_data(*vd, GL_TRIANGLES, 3);
        }
        fbo.unbind();

        tex_cube->bind();
        gl(glGenerateMipmap, tex_cube->get_params().type);
    }

    // Diffuse (convolution)
    {
        const int size = int(resolution);
        auto& shader = *get_shader(r, shader_convolve);

        fbo.bind();
        tex_diffuse->resize(size, size);
        shader.bind();

        shader["M"] = M;
        shader["NMat"] = M;

        for (auto i = 0; i < 6; i++) {
            fbo.set_color_attachement(0, tex_diffuse, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i);
            gl(glViewport, 0, 0, size, size);
            gl(glClearColor, 0.0f, 0.0f, 0.0f, 0.0f);
            gl(glClear, GL_COLOR_BUFFER_BIT);

            Eigen::Matrix4f PV = (P * V[i]);
            shader["PV"] = PV;

            tex_cube->bind_to(GL_TEXTURE0);
            shader["texCube"] = 0;
            render_vertex_data(*vd, GL_TRIANGLES, 3);
        }

        tex_diffuse->bind();
        gl(glGenerateMipmap, tex_diffuse->get_params().type);
    }

    // Specular
    {
        const int size = int(resolution);
        auto& shader = *get_shader(r, shader_specular);

        fbo.bind();
        tex_specular->resize(size, size);
        fbo.check_status();

        shader.bind();

        shader["M"] = M;
        shader["NMat"] = M;
        shader["sampleCount"] = int(4096);
        shader["roughness"] = 0.0f;

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

Entity add_ibl(Registry& registry, IBL ibl)
{
    auto entity = create_scene_node(registry, "IBL");
    registry.emplace<IBL>(entity, std::move(ibl));
    return entity;
}

void clear_ibl(Registry& registry)
{
    auto v = registry.view<IBL>();
    registry.destroy(v.begin(), v.end());
}

bool save_ibl(const IBL& ibl, const fs::path& folder)
{
    if (!ibl.background_rect || !ibl.background || !ibl.diffuse || !ibl.specular) {
        lagrange::logger().error("save_ibl: Invalid IBL");
        return false;
    }

    if (!fs::exists(folder)) {
        lagrange::logger().error("save_ibl: Folder {} does not exist", folder.string());
        return false;
    }

    const int levels = static_cast<int>(std::log2(ibl.specular->get_width()));


    ibl.background_rect->save_to(folder / "background_rect.png", GL_TEXTURE_2D);
    for (auto i = 0; i < 6; i++) {
        const auto cube_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + i;
        ibl.background->save_to(folder / string_format("background_{:02d}.png", i), cube_target);
        ibl.diffuse->save_to(folder / string_format("diffuse_{:02d}.png", i), cube_target);
        for (auto mip_level = 0; mip_level < levels; mip_level++) {
            ibl.specular->save_to(
                folder / string_format("specular_{:02d}_mip_{:02}.png", i, mip_level),
                cube_target,
                100,
                mip_level);
        }
    }
    return true;
}


} // namespace ui
} // namespace lagrange

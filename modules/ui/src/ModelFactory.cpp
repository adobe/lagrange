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
#include <lagrange/ui/ModelFactory.h>

#include <cctype>

namespace lagrange {
namespace ui {

std::shared_ptr<lagrange::ui::Material> ModelFactory::convert_material(
    const fs::path& base_dir, const tinyobj::material_t& tinymat)
{
    auto try_texture_load = [&](std::string suffix, const Texture::Params& param = Texture::Params()) -> Resource<Texture> {
        suffix[0] = std::tolower(suffix[0]);

        auto capitalized = suffix;
        capitalized[0] = std::toupper(suffix[0]);

        const fs::path opts[] = {
            base_dir / (tinymat.name + "_" + suffix + ".png"),
            base_dir / (tinymat.name + "_" + suffix + ".png"),
            base_dir / (tinymat.name + "-" + suffix + ".png"),
            base_dir / (suffix + ".png"),
            base_dir / (tinymat.name + "_" + capitalized + ".png"),
            base_dir / (tinymat.name + "_" + capitalized + ".png"),
            base_dir / (tinymat.name + "-" + capitalized + ".png"),
            base_dir / (capitalized + ".png"),
        };

        for (auto& filename : opts) {
            if (fs::exists(filename)) {
                logger().debug("Discovered texture: {}", filename);
                return Resource<Texture>::create(filename, param);
            }
        }

        return Resource<Texture>();
    };


    auto mat_ptr = std::make_shared<Material>(Material::create_default(Material::MATERIAL_ADOBE_STANDARD));
    mat_ptr->set_name(tinymat.name);

    auto& m = *mat_ptr;

    //Assuming sRGB for base color
    Texture::Params base_param;
    base_param.sRGB = true;

    m["baseColor"].value = { float(tinymat.diffuse[0]), float(tinymat.diffuse[1]), float(tinymat.diffuse[2]) };
    if (tinymat.diffuse_texname.length() > 0) {
        m["baseColor"].texture = Resource<Texture>::create(base_dir / tinymat.diffuse_texname, base_param);
    }
    else {
        m["baseColor"].texture = try_texture_load("baseColor", base_param);
        m["baseColor"].texture = try_texture_load("base_color", base_param);
        m["baseColor"].texture = try_texture_load("diffuse", base_param);
        m["baseColor"].texture = try_texture_load("albedo", base_param);
    }

    m["glow"].value = { float(tinymat.emission[0]), float(tinymat.emission[1]), float(tinymat.emission[2]) };
    if (tinymat.emissive_texname.length() > 0) {
        m["glow"].texture = Resource<Texture>::create(base_dir / tinymat.emissive_texname);
    }

    m["opacity"].value = Color{ 1.0f };
    if (tinymat.alpha_texname.length() > 0) {
        m["opacity"].texture = Resource<Texture>::create(base_dir / tinymat.alpha_texname);
    }

    m["roughness"].value = { float(tinymat.roughness) };

    if (tinymat.roughness_texname.length() > 0) {
        m["roughness"].texture = Resource<Texture>::create(base_dir / tinymat.roughness_texname);
    }
    else {
        m["roughness"].texture = try_texture_load("roughness");
    }

    m["metallic"].value = { float(tinymat.metallic) };
    if (tinymat.metallic_texname.length() > 0) {
        m["metallic"].texture = Resource<Texture>::create(base_dir / tinymat.metallic_texname);
    }
    else {
        m["metallic"].texture = try_texture_load("glossiness");
        if (m["metallic"].texture) {
            logger().warn("TODO invert glosiness to rougness");
        }
        m["metallic"].texture = try_texture_load("metallic");
        m["metallic"].texture = try_texture_load("metalness");
    }

    m["translucence"].value = Color{ 0.0f };
    m["indexOfRefraction"].value = { float(tinymat.ior) };
    m["density"].value = Color{ 1.0f };
    m["interiorColor"].value = Color{ 1.0f, 1.0f, 1.0f };

    m["height"].value = Color{ 0.0f };
    if (tinymat.displacement_texname.length() > 0) {
        m["height"].texture = Resource<Texture>::create(base_dir / tinymat.displacement_texname);
    }
    else {
        m["height"].texture = try_texture_load("height");
        m["height"].texture = try_texture_load("displacement");
    }

    m["heightScale"].value = Color{ 1.0f };

    m["normal"].value = Color{ 0.0f,0.0f,0.0f,0.0f };
    if (tinymat.normal_texname.length() > 0) {
        m["normal"].texture = Resource<Texture>::create(base_dir / tinymat.normal_texname);
    }
    else {
        m["normal"].texture = try_texture_load("normal");
    }

    return mat_ptr;
}



Callbacks<
    ModelFactory::OnModelLoad,
    ModelFactory::OnModelSave
> ModelFactory::m_callbacks;

}
}

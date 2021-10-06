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

#include <lagrange/ui/default_entities.h>
#include <lagrange/ui/default_shaders.h>
#include <lagrange/ui/types/Material.h>
#include <lagrange/ui/utils/io.h>
#include <lagrange/ui/utils/treenode.h>

#ifdef LAGRANGE_WITH_ASSIMP
#include <assimp/material.h>
#include <assimp/pbrmaterial.h>
#endif

namespace lagrange {
namespace ui {


std::shared_ptr<Texture> load_texture(const fs::path& path, const Texture::Params& params)
{
    try {
        return std::make_shared<Texture>(path, params);
    } catch (const std::runtime_error& ex) {
        lagrange::logger().error("{}", ex.what());
        return nullptr;
    }
}

std::shared_ptr<Texture> discover_texture(
    const fs::path& base_dir,
    const std::string& name,
    const std::string& suffix,
    const Texture::Params& param = Texture::Params())
{
    std::string lower = suffix;
    lower[0] = std::tolower(suffix[0]);

    auto capitalized = suffix;
    capitalized[0] = std::toupper(suffix[0]);

    const fs::path opts[] = {
        base_dir / (name + "_" + lower + ".png"),
        base_dir / (name + "_" + lower + ".png"),
        base_dir / (name + "-" + lower + ".png"),
        base_dir / (lower + ".png"),
        base_dir / (name + "_" + capitalized + ".png"),
        base_dir / (name + "_" + capitalized + ".png"),
        base_dir / (name + "-" + capitalized + ".png"),
        base_dir / (capitalized + ".png"),
    };

    for (auto& filename : opts) {
        if (fs::exists(filename)) {
            lagrange::logger().debug("Discovered texture: {}", filename);
            return std::make_shared<Texture>(filename, param);
        }
    }

    return nullptr;
}

std::shared_ptr<lagrange::ui::Material>
convert_material(Registry& r, const fs::path& base_dir, const tinyobj::material_t& tinymat)
{
    auto mat_ptr = create_material(r, DefaultShaders::PBR);
    auto& m = *mat_ptr;

    // Assuming sRGB for base color
    Texture::Params base_param;
    base_param.sRGB = true;

    m.set_color(
        PBRMaterial::BaseColor,
        {float(tinymat.diffuse[0]), float(tinymat.diffuse[1]), float(tinymat.diffuse[2])});
    {
        std::shared_ptr<Texture> tex = nullptr;
        if (tinymat.diffuse_texname.length() > 0) {
            tex = load_texture(base_dir / tinymat.diffuse_texname, base_param);
        }
        if (!tex) tex = discover_texture(base_dir, tinymat.name, "baseColor", base_param);
        if (!tex) tex = discover_texture(base_dir, tinymat.name, "base_color", base_param);
        if (!tex) tex = discover_texture(base_dir, tinymat.name, "diffuse", base_param);
        if (!tex) tex = discover_texture(base_dir, tinymat.name, "albedo", base_param);

        if (tex) m.set_texture(PBRMaterial::BaseColor, tex);
    }


    // No emissive in PBR material
    // skipping
    // tinymat.emission


    m.set_float(PBRMaterial::Opacity, 1.0f);
    if (tinymat.alpha_texname.length() > 0) {
        m.set_texture(PBRMaterial::Opacity, load_texture(base_dir / tinymat.alpha_texname));
    }

    m.set_float(PBRMaterial::Roughness, float(tinymat.roughness));
    {
        std::shared_ptr<Texture> tex = nullptr;
        if (tinymat.roughness_texname.length() > 0) {
            tex = load_texture(base_dir / tinymat.roughness_texname);
        }
        if (!tex) tex = discover_texture(base_dir, tinymat.name, "roughness");
        if (!tex) {
            tex = discover_texture(base_dir, tinymat.name, "glossiness");
            if (tex) {
                lagrange::logger().warn("TODO invert glossiness to roughness. utils/io.cpp");
            }
        }
        if (tex) m.set_texture(PBRMaterial::Roughness, tex);
    }


    m.set_float(PBRMaterial::Metallic, float(tinymat.metallic));
    {
        std::shared_ptr<Texture> tex = nullptr;
        if (tinymat.metallic_texname.length() > 0) {
            tex = load_texture(base_dir / tinymat.metallic_texname);
        }
        if (!tex) tex = discover_texture(base_dir, tinymat.name, "metallic");
        if (!tex) tex = discover_texture(base_dir, tinymat.name, "metalness");
        if (tex) m.set_texture(PBRMaterial::Metallic, tex);
    }


    m.set_float(PBRMaterial::Metallic, float(tinymat.metallic));

    m.set_float("indexOfRefraction", float(tinymat.ior));


    m.set_float("height", 0.0f);
    {
        std::shared_ptr<Texture> tex = nullptr;
        if (tinymat.displacement_texname.length() > 0) {
            tex = load_texture(base_dir / tinymat.displacement_texname);
        }
        if (!tex) tex = discover_texture(base_dir, tinymat.name, "height");
        if (!tex) tex = discover_texture(base_dir, tinymat.name, "displacement");
        if (tex) m.set_texture("height", tex);
    }
    m.set_float("heightScale", 0.0f);


    {
        std::shared_ptr<Texture> tex = nullptr;
        if (tinymat.normal_texname.length() > 0) {
            tex = load_texture(base_dir / tinymat.normal_texname);
        }
        if (!tex) tex = discover_texture(base_dir, tinymat.name, "normal");
        if (tex) m.set_texture(PBRMaterial::Normal, tex);
    }

    return mat_ptr;
}


#ifdef LAGRANGE_WITH_ASSIMP

Entity detail::load_scene_impl(
    Registry& r,
    const aiScene* scene,
    const fs::path& path,
    const std::vector<Entity>& meshes)
{
    LA_ASSERT(scene);


    const auto parent_path = path.parent_path();

    const auto resolve_texture_path = [&](const aiString& tex_path) -> lagrange::fs::path {
        const auto p = lagrange::fs::path(tex_path.C_Str());

        // As is
        if (lagrange::fs::exists(p)) {
            return p;
        }

        // Relative
        if (p.is_relative()) {
            const auto tmp_path = parent_path / p;
            if (lagrange::fs::exists(tmp_path)) return tmp_path;
        }

        // Absolute, different fs
        if (p.is_absolute()) {
            const auto tmp_path = parent_path / p.filename();
            if (lagrange::fs::exists(tmp_path)) return tmp_path;
        }

        lagrange::logger().warn("{} is an invalid texture path", p);
        return fs::path();
    };


    static_assert(std::is_same_v<ai_real, float>);


    std::vector<std::shared_ptr<Material>> materials;
    for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
        const auto* amat = scene->mMaterials[i];

        auto mat = create_material(r, DefaultShaders::PBR);


        aiString matname;
        amat->Get(AI_MATKEY_NAME, matname);


        // Base color
        {
            aiColor4D color(0.f, 0.f, 0.f, 1.f);
            if (amat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == aiReturn_SUCCESS) {
                ui::Color c;
                c.r() = color.r;
                c.g() = color.g;
                c.b() = color.b;
                c.a() = color.a;
                mat->set_color(PBRMaterial::BaseColor, c);

                lagrange::logger().trace("Material index {} name {} base color {}",i, matname.C_Str(), c);
            }
        }
        // Base color texture
        {
            aiString dpath;
            if (amat->Get(AI_MATKEY_TEXTURE_DIFFUSE(0), dpath) == aiReturn_SUCCESS &&
                dpath.length > 0) {
                mat->set_texture(PBRMaterial::BaseColor, load_texture(resolve_texture_path(dpath)));
            }

            if (amat->Get(AI_MATKEY_TEXTURE(aiTextureType_BASE_COLOR, 0), dpath) ==
                    aiReturn_SUCCESS &&
                dpath.length > 0) {
                mat->set_texture(PBRMaterial::BaseColor, load_texture(resolve_texture_path(dpath)));
            }
        }

        // Normal texture
        {
            aiString dpath;
            if (amat->Get(AI_MATKEY_TEXTURE(aiTextureType_NORMALS, 0), dpath) == aiReturn_SUCCESS &&
                dpath.length > 0) {
                mat->set_texture(PBRMaterial::Normal, load_texture(resolve_texture_path(dpath)));
            }
        }

        // Roughness texture
        {
            aiString dpath;
            if (amat->Get(AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE_ROUGHNESS, 0), dpath) ==
                    aiReturn_SUCCESS &&
                dpath.length > 0) {
                mat->set_texture(PBRMaterial::Roughness, load_texture(resolve_texture_path(dpath)));
            }
        }

        // Metallic texture
        {
            aiString dpath;
            if (amat->Get(AI_MATKEY_TEXTURE(aiTextureType_METALNESS, 0), dpath) ==
                    aiReturn_SUCCESS &&
                dpath.length > 0) {
                mat->set_texture(PBRMaterial::Metallic, load_texture(resolve_texture_path(dpath)));
            }
        }

        {
            float opacity = 1.0f;
            if (amat->Get(AI_MATKEY_OPACITY, opacity) == aiReturn_SUCCESS) {
                mat->set_float(PBRMaterial::Opacity, std::max(0.0f, std::min(1.0f, opacity)));
            }
        }


        for (size_t i = 0; i < amat->mNumProperties; i++) {
            auto prop = amat->mProperties[i];
            lagrange::logger().trace(
                "key: {} length: {}, type: {:02X}, index: {}",
                prop->mKey.C_Str(),
                prop->mDataLength,
                int(prop->mType),
                prop->mIndex);
        }


        materials.push_back(mat);
    }


    std::function<Entity(Entity, aiNode*)> walk;


    walk = [&](Entity parent, aiNode* node) -> Entity {
        if (!node) return NullEntity;

        Entity e = ui::create_scene_node(r, node->mName.C_Str());
        ui::set_parent(r, e, parent);

        // Transform
        Eigen::Matrix4f t;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                t(i, j) = node->mTransformation[i][j];
            }
        }


        for (unsigned int mesh_index = 0; mesh_index < node->mNumMeshes; mesh_index++) {
            // Mesh
            auto mesh_e = ui::show_mesh(
                r,
                meshes[node->mMeshes[mesh_index]],
                DefaultShaders::PBR); // todo PBRSkeletal if has bones

            lagrange::logger().trace(
                "Mesh index {}, name parent {}, material index {}",
                mesh_index,
                node->mName.C_Str(),
                scene->mMeshes[node->mMeshes[mesh_index]]->mMaterialIndex);

            // Material
            ui::set_material(
                r,
                mesh_e,
                materials[scene->mMeshes[node->mMeshes[mesh_index]]->mMaterialIndex]);
            ui::set_parent(r, mesh_e, e);
        }

        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            walk(e, node->mChildren[i]);
        }


        return e;
    };


    auto root = walk(NullEntity, scene->mRootNode);
    return root;
}

#endif

} // namespace ui
} // namespace lagrange

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
#pragma once

#include <lagrange/Logger.h>
#include <lagrange/ui/Entity.h>
#include <lagrange/ui/components/MeshData.h>
#include <lagrange/ui/types/Texture.h>
#include <lagrange/ui/utils/io_assimp.h>

#include <lagrange/io/load_mesh_ext.h>

#include <sstream>

namespace lagrange {
namespace ui {

std::shared_ptr<Texture> load_texture(
    const fs::path& path,
    const Texture::Params& params = Texture::Params());

/// @brief Convertrs tinyobj's material_t to UI's Material
/// @param base_dir
/// @param tinymat
/// @return
std::shared_ptr<Material>
convert_material(Registry& r, const fs::path& base_dir, const tinyobj::material_t& tinymat);


/// Loads obj as a single mesh. Creates entity with MeshData component.
/// Use show_mesh to add it to scene
template <typename MeshType>
Entity load_obj(
    Registry& r,
    const fs::path& path,
    const io::MeshLoaderParams& params = io::MeshLoaderParams())
{
    if (path.extension() != ".obj") {
        lagrange::logger().error("wrong file format '{}' for load_obj", path.extension());
        return NullEntity;
    }

    // Override to load as one mesh
    auto p = params;
    p.as_one_mesh = true;
    p.load_materials = false;
    p.triangulate = true;

    auto res = lagrange::io::load_mesh_ext<MeshType>(path, params);
    if (!res.success) return NullEntity;

    auto mesh = lagrange::to_shared_ptr(std::move(res.meshes.front()));
    auto e = register_mesh<MeshType>(r, std::dynamic_pointer_cast<MeshType>(mesh));
    ui::set_name(r, e, path.filename().string());

    return e;
}

/// Loads obj as a single mesh and materials. Creates entity with MeshData component.
/// Use show_mesh to add it to scene
/// Returns entity and array of material pointers
template <typename MeshType>
std::pair<Entity, std::vector<std::shared_ptr<Material>>> load_obj_with_materials(
    Registry& r,
    const fs::path& path,
    const io::MeshLoaderParams& params = io::MeshLoaderParams())
{
    if (path.extension() != ".obj") {
        lagrange::logger().error("wrong file format '{}' for load_obj", path.extension());
        return {NullEntity, {}};
    }

    // Override to load as one mesh
    auto p = params;
    p.as_one_mesh = true;
    p.load_materials = true;
    p.triangulate = true;

    auto res = lagrange::io::load_mesh_ext<MeshType>(path, params);
    if (!res.success) return {NullEntity, {}};

    auto mesh = lagrange::to_shared_ptr(std::move(res.meshes.front()));
    auto e = register_mesh<MeshType>(r, std::dynamic_pointer_cast<MeshType>(mesh));
    ui::set_name(r, e, path.filename().string());

    std::vector<std::shared_ptr<Material>> mats;
    auto base_dir = path;
    base_dir.remove_filename();

    for (auto& tinymat : res.materials) {
        mats.push_back(convert_material(r, base_dir, tinymat));
    }

    return {e, mats};
}

} // namespace ui
} // namespace lagrange

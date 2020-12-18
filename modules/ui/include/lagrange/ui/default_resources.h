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

#include <lagrange/fs/file_utils.h>
#include <lagrange/ui/Emitter.h>
#include <lagrange/ui/FrameBuffer.h>
#include <lagrange/ui/MeshBuffer.h>
#include <lagrange/ui/Resource.h>
#include <lagrange/ui/Shader.h>
#include <lagrange/ui/Texture.h>


// Mesh specific
#include <lagrange/Mesh.h>
#include <lagrange/io/load_mesh_ext.h>
#include <lagrange/ui/Material.h>
#include <lagrange/ui/ProxyMesh.h>

#include <memory>

namespace lagrange {
namespace ui {


/// Makes available following resources
///     Resource<File>::create(path)
///     Resource<Texture>::create(std::string/const char*/fs::path,const Texture::Params& = {})
///     Resource<FrameBuffer>::create(const FBOResourceParams &)
///     Resource<Shader>::create(const ShaderResourceParams &)
///     Resource<MDL::Library>::create(path)
///     Resource<Material>::create(base_dir, tinyobj::material_t)
///     Resource<ObjResult<Vertices3Df,Triangles>>::create(path to obj)
///     Resource<MeshBase>::create(Vertices3Df &&, Triangles &&)
void register_default_resources();

/// Makes available following resources
///     Resource<ObjResult<Vertices,Facets>>::create(path to obj), path can be fs::path, std::string or const char *
///     Resource<MeshBase>::create(V &&, F &&)
template <typename V, typename F>
void register_mesh_resource();

/// File resource
struct File
{
    lagrange::fs::path path;
    using Timestamp = lagrange::fs::file_time_type;
    Timestamp timestamp;
};

/// Base type for .obj loading result
struct ObjResultBase
{
    std::vector<Resource<MeshBase>> meshes;
    std::vector<std::vector<std::pair<Resource<Material>, int>>> mesh_to_material;
};

/// .obj loading result for vertices type V and facets F
template <typename V, typename F>
struct ObjResult : public ObjResultBase
{
};


/// Framebuffer creation parameters
struct FBOResourceParams
{
    Resource<Texture> color_attachment_0 = {};
    Resource<Texture> depth_attachment = {};
    unsigned int color_type_override = 0;
    unsigned int depth_type_override = 0;

    // If managed is false, set custom_id to create non-owning FBO wrapper
    bool managed = true;
    GLuint custom_id = 0;
    // Id 1 and higher
    std::vector<Resource<Texture>> additional_color_attachments = {};
};

/// Shader creation parameters
struct ShaderResourceParams
{
    enum class Tag {
        VIRTUAL_PATH, // For default shaders included in the binary
        REAL_PATH,
        CODE_ONLY
    };
    Tag tag;
    std::string path = "";
    std::string source = "";
    std::vector<std::pair<std::string, std::string>> defines = {};
};


/// Emitter render data
struct EmitterRenderData
{
    Emitter* emitter = nullptr;
    Resource<Texture> shadow_map = {};
    Eigen::Matrix4f PV = Eigen::Matrix4f::Identity();
    float shadow_near = 0.0f;
    float shadow_far = 1.0f;
};

///////////////////////////////////////////////////////////////////////

/// Helper function for loading materials from texture, either from MDL or from .mtl
std::unordered_map<std::string, Resource<Material>> resolve_materials(
    const fs::path& base_dir,
    const fs::path& file_stem,
    const std::vector<tinyobj::material_t>& tinymats);

/// Implementation
template <typename V, typename F>
void register_mesh_resource()
{
    // Instantiate template for proxy mesh
    ResourceFactory::register_resource_factory([](ResourceData<ProxyMesh>& data,
                                                  Resource<MeshBase> mesh,
                                                  Mesh<V, F>* type /* = nullptr*/) {
        using MeshType = Mesh<V, F>;
        LA_IGNORE(type);
        data.set(std::make_unique<ProxyMesh>(dynamic_cast<MeshType&>(*mesh)));
        data.add_dependency(mesh.data());
    });

    ResourceFactory::register_resource_factory([](ResourceData<ObjResult<V, F>>& data,
                                                  const fs::path& path,
                                                  const io::MeshLoaderParams& params) {
        const fs::path base_dir = path.parent_path();
        const fs::path file_stem = path.stem();
        const fs::path file_name = path.filename();

        using MeshType = Mesh<V, F>;

        // Create file resource
        auto file = Resource<File>::create(path);

        // Load obj data
        auto res = lagrange::io::load_mesh_ext<MeshType>(path, params);

        // Create a name -> Material map
        std::unordered_map<std::string, Resource<Material>> name_to_material =
            resolve_materials(base_dir, file_stem, res.materials);

        // Initialize result object
        auto obj_result = std::make_unique<ObjResult<V, F>>();

        // Initialize mesh to material array
        obj_result->mesh_to_material.resize(res.meshes.size());

        for (size_t mesh_index = 0; mesh_index < res.meshes.size(); mesh_index++) {
            auto& mesh = res.meshes[mesh_index];

            // Has mat id attribute
            if (mesh->has_facet_attribute("material_id")) {
                auto& mat_ids = mesh->get_facet_attribute("material_id");

                // Create a set of ids that are used in this mesh
                // TODO: move this to load_mesh_ext
                std::unordered_set<int> used_mats;
                for (auto i = 0; i < mat_ids.size(); i++) {
                    used_mats.insert(static_cast<int>(mat_ids(i)));
                }

                // Assign used materials to mesh
                for (auto material_id : used_mats) {
                    // Skip invalid IDs
                    if (material_id == -1 || material_id >= int(res.materials.size())) {
                        continue;
                    }

                    auto it = name_to_material.find(res.materials[material_id].name);

                    // Check if it is in mat library
                    if (it == name_to_material.end()) {
                        continue;
                    }

                    // Assign to mesh
                    obj_result->mesh_to_material[mesh_index].push_back(
                        std::make_pair(it->second, material_id));
                }
            }
        }

        // Create MeshBase resources
        for (auto& mesh_ptr : res.meshes) {
            obj_result->meshes.push_back(Resource<MeshBase>::create(std::move(mesh_ptr)));
        }

        // Set mesh dependencies
        for (auto& mesh_res : obj_result->meshes) {
            data.add_dependency(mesh_res.data());
        }

        // Set material dependencies
        for (auto& it : name_to_material) {
            data.add_dependency(it.second.data());
        }

        data.set(std::move(obj_result));
    });

    // std::string overload
    ResourceFactory::register_resource_factory([](ResourceData<ObjResult<V, F>>& data,
                                                  const std::string& path,
                                                  const io::MeshLoaderParams& params) {
        data.set(Resource<ObjResult<V, F>>::create(fs::path(path), params).data()->data());
    });

    ResourceFactory::register_resource_factory(
        [](ResourceData<MeshBase>& data, V&& vertices, F&& facets) {
            data.set(lagrange::create_mesh(std::move(vertices), std::move(facets)));
        });
}


} // namespace ui
} // namespace lagrange

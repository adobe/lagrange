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
#include <lagrange/fs/file_utils.h>
#include <lagrange/ui/ModelFactory.h>
#include <lagrange/ui/default_resources.h>
#include <lagrange/utils/la_assert.h>

#include <lagrange/create_mesh.h>
#include <lagrange/ui/MDL.h>

#include <lagrange/ui/MeshBufferFactory.h>

namespace lagrange {
namespace ui {

#ifndef DEFAULT_SHADERS_USE_REAL_PATH
// Contains virtual fs with shaders: VIRTUAL_SHADERS
#include <shaders.inc>
#endif


void register_default_resources()
{
    // File resource from fs::path
    ResourceFactory::register_resource_factory([](ResourceData<File>& data, const fs::path& path) {
        // Fail if file doesn't exist
        if (!fs::exists(path)) return;

        auto f = std::make_unique<File>();
        f->path = path;
        f->timestamp = lagrange::fs::last_write_time(f->path);
        data.set(std::move(f));
    });

    // File resource std::string overload
    ResourceFactory::register_resource_factory(
        [](ResourceData<File>& data, const std::string& path) {
            data.set(Resource<File>::create(fs::path(path)).data()->data());
        });


    ResourceFactory::register_resource_factory(
        [](ResourceData<Texture>& data, const std::string& path, const Texture::Params& params) {
            if (!GLState::on_opengl_thread()) return;

            if (fs::path(path).extension() == ".exr") {
                lagrange::logger().warn("EXR textures are not currently supported");
                return;
            }

            // Create file resource
            auto file = Resource<File>::create(path);

            // Create texture from file path
            data.set(std::make_unique<Texture>(file->path, params));

            // Add dependency on the file
            data.add_dependency(file.data());
        });

    ResourceFactory::register_resource_factory(
        [](ResourceData<Texture>& data, const std::string& path) {
            if (!GLState::on_opengl_thread()) return;

            if (fs::path(path).extension() == ".exr") {
                lagrange::logger().warn("EXR textures are not currently supported");
                return;
            }

            // Create file resource
            auto file = Resource<File>::create(path);

            // Create texture from file path
            data.set(std::make_unique<Texture>(file->path));

            // Add dependency on the file
            data.add_dependency(file.data());
        });

    ResourceFactory::register_resource_factory(
        [](ResourceData<Texture>& data, const fs::path& path) {
            if (!GLState::on_opengl_thread()) return;

            if (path.extension() == ".exr") {
                lagrange::logger().warn("EXR textures are not currently supported");
                return;
            }

            // Create file resource
            auto file = Resource<File>::create(path.string());

            // Create texture from file path
            data.set(std::make_unique<Texture>(file->path));

            // Add dependency on the file
            data.add_dependency(file.data());
        });


    ResourceFactory::register_resource_factory(
        [](ResourceData<FrameBuffer>& data, const FBOResourceParams& params) {
            if (!GLState::on_opengl_thread()) return;

            if (params.managed == false) {
                data.set(std::make_unique<FrameBuffer>(params.custom_id));
                return;
            }

            auto ptr = std::make_unique<FrameBuffer>();


            if (params.color_attachment_0.has_value()) {
                GLenum type = (params.color_type_override == GL_NONE)
                                  ? params.color_attachment_0->get_params().type
                                  : params.color_type_override;

                ptr->set_color_attachement(0, params.color_attachment_0, type);
                data.add_dependency(params.color_attachment_0.data());
            }
            if (params.depth_attachment.has_value()) {
                GLenum type = (params.depth_type_override == GL_NONE)
                                  ? params.depth_attachment->get_params().type
                                  : params.depth_type_override;
                ptr->set_depth_attachement(params.depth_attachment, type);
                data.add_dependency(params.depth_attachment.data());
            }

            int additional_cnt = 0;
            for (auto attachment : params.additional_color_attachments) {
                ptr->set_color_attachement(
                    1 + additional_cnt,
                    attachment,
                    attachment->get_params().type);
                data.add_dependency(attachment.data());
                additional_cnt++;
            }

            ptr->check_status();

            data.set(std::move(ptr));
        });


    ResourceFactory::register_resource_factory(
        [](ResourceData<Shader>& data, const ShaderResourceParams& params) {
            if (!GLState::on_opengl_thread()) return;

            std::unique_ptr<Shader> shader = nullptr;

            if (params.tag == ShaderResourceParams::Tag::CODE_ONLY) {
                try {
                    shader = std::make_unique<Shader>(params.source, params.defines);
                } catch (ShaderException& ex) {
                    ex.set_desc("CODE");
                    std::rethrow_exception(std::current_exception());
                }
            }
            if (params.tag == ShaderResourceParams::Tag::REAL_PATH) {
                // Create file resource
                auto file = Resource<File>::create(params.path);

                try {
                    shader = std::make_unique<Shader>(
                        fs::read_file_with_includes("", file->path),
                        params.defines);
                } catch (ShaderException& ex) {
                    ex.set_desc(file->path.string());
                    std::rethrow_exception(std::current_exception());
                }

                data.add_dependency(file.data());
            }
            if (params.tag == ShaderResourceParams::Tag::VIRTUAL_PATH) {
#ifdef DEFAULT_SHADERS_USE_REAL_PATH
                try {
                    shader = std::make_unique<Shader>(
                        utils::strings::read_file_with_includes(
                            std::string(DEFAULT_SHADERS_USE_REAL_PATH),
                            params.file_path),
                        params.defines);
                } catch (ShaderException& ex) {
                    ex.set_desc(params.file_path->string());
                    std::rethrow_exception(std::current_exception());
                }
#else
                try {
                    shader = std::make_unique<Shader>(
                        fs::read_file_with_includes(params.path, VIRTUAL_SHADERS),
                        params.defines);
                } catch (ShaderException& ex) {
                    ex.set_desc("VIRTUAL/params.file_path_or_source");
                    std::rethrow_exception(std::current_exception());
                }
#endif
            }

            data.set(std::move(shader));
        });


    // MDL library
    ResourceFactory::register_resource_factory(
        [](ResourceData<MDL::Library>& data, const std::string& path) {
            auto file = Resource<File>::create(path);
            if (!file.has_value()) return;

            auto fs_path = fs::path(path);
            const fs::path base_dir = fs_path.parent_path();
            const fs::path file_stem = fs_path.stem();
            const fs::path file_name = fs_path.filename();

            auto lib = MDL::get_instance().load_materials(base_dir, file_stem.string());

            if (lib.materials.size() == 0) return;

            data.set(std::make_unique<MDL::Library>(std::move(lib)));
            data.add_dependency(file.data());
        });


    // tiny_obj to Material
    ResourceFactory::register_resource_factory([](ResourceData<Material>& data,
                                                  const fs::path& base_dir,
                                                  const tinyobj::material_t& tinymat) {
        // Todo remove from model factory and put here
        data.set(ModelFactory::convert_material(base_dir, tinymat));
    });


    /// Creates a MeshBuffer from ProxyMesh
    /// The resulting MeshBuffer contains
    ///     1. vertex positions         (flattened)
    ///     2. indices for vertices
    ///     3. indices for edges
    ///     4. normals                  (flattened, optional)
    ///     5. uvs                      (flattened, optional)
    ///     6. tangents                 (flattened, optional)
    ///     7. bitangents               (flattened, optional)
    ResourceFactory::register_resource_factory([](ResourceData<MeshBuffer>& data,
                                                  Resource<ProxyMesh> proxy_resource) {
        if (!GLState::on_opengl_thread()) return;

        std::unique_ptr<MeshBuffer> buffer_ptr = std::make_unique<MeshBuffer>();
        auto& buffer = *buffer_ptr;

        auto& proxy = *proxy_resource;

        const auto& V = proxy.get_vertices();
        const auto& proxy_mesh = proxy.mesh();

        // Vertex indices for drawing points
        {
            buffer.get_sub_buffer(MeshBuffer::SubBufferType::INDICES, MeshBuffer::vertex_index_id())
                .upload(proxy.get_vertex_to_vertex_mapping());
        }

        // Edge indices for drawing lines
        {
            buffer.get_sub_buffer(MeshBuffer::SubBufferType::INDICES, MeshBuffer::edge_index_id())
                .upload(proxy.get_edge_to_vertices());
        }

        // Position
        {
            MeshBufferFactory::upload_vertex_attribute(
                buffer.get_sub_buffer(MeshBuffer::SubBufferType::POSITION),
                V,
                proxy);
        }

        // Normal
        if (proxy_mesh.has_corner_attribute("normal")) {
            buffer.get_sub_buffer(MeshBuffer::SubBufferType::NORMAL)
                .upload(proxy_mesh.get_corner_attribute("normal"));
        } else if (proxy_mesh.has_vertex_attribute("normal")) {
            MeshBufferFactory::upload_vertex_attribute(
                buffer.get_sub_buffer(MeshBuffer::SubBufferType::NORMAL),
                proxy_mesh.get_vertex_attribute("normal"),
                proxy);
        }

        // UV
        if (proxy_mesh.has_corner_attribute("uv")) {
            buffer.get_sub_buffer(MeshBuffer::SubBufferType::UV)
                .upload(proxy_mesh.get_corner_attribute("uv"));
        }
        if (proxy_mesh.has_corner_attribute("tangent")) {
            buffer.get_sub_buffer(MeshBuffer::SubBufferType::TANGENT)
                .upload(proxy_mesh.get_corner_attribute("tangent"));
        }
        if (proxy_mesh.has_corner_attribute("bitangent")) {
            buffer.get_sub_buffer(MeshBuffer::SubBufferType::BITANGENT)
                .upload(proxy_mesh.get_corner_attribute("bitangent"));
        }

        // Material ID buffers (applicable to triangle drawing only)
        const auto& mat_indices = proxy.get_material_indices();

        if (mat_indices.size() > 0) {
            for (const auto& it : mat_indices) {
                std::vector<unsigned int> flattened;
                flattened.reserve(it.second.size() * 3);
                for (auto triangle_index : it.second) {
                    flattened.push_back(triangle_index * 3 + 0);
                    flattened.push_back(triangle_index * 3 + 1);
                    flattened.push_back(triangle_index * 3 + 2);
                }

                buffer
                    .get_sub_buffer(
                        MeshBuffer::SubBufferType::INDICES,
                        MeshBuffer::material_index_id(it.first))
                    .upload(flattened);
            }
        }


        data.set(std::move(buffer_ptr));
        data.add_dependency(proxy_resource.data());
    });


    // Register triangle3df mesh
    register_mesh_resource<lagrange::Vertices3Df, lagrange::Triangles>();
}

std::unordered_map<std::string, lagrange::ui::Resource<lagrange::ui::Material>> resolve_materials(
    const fs::path& base_dir,
    const fs::path& file_stem,
    const std::vector<tinyobj::material_t>& tinymats)
{
    std::unordered_map<std::string, Resource<Material>> name_to_material;

    // Try to load MDL library
    const auto mdl_path = (base_dir / (file_stem.string() + ".mdl")).string();
    auto mdl = Resource<MDL::Library>::create(mdl_path);
    if (mdl) {
        for (const auto& it : mdl->materials) {
            name_to_material[it.first] = Resource<Material>::create(it.second);
        }
    }
    // Otherwise use tinyobj materials
    else {
        for (const auto& mat : tinymats) {
            auto mat_resource = Resource<Material>::create(base_dir, mat);
            name_to_material[mat_resource->get_name()] = std::move(mat_resource);
        }
    }

    return name_to_material;
}

} // namespace ui
} // namespace lagrange

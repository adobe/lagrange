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

#include <lagrange/ui/components/GLMesh.h>
#include <lagrange/ui/components/MeshData.h>
#include <lagrange/ui/components/MeshGeometry.h>
#include <lagrange/ui/components/MeshRender.h>
#include <lagrange/ui/components/Common.h>
#include <lagrange/ui/systems/update_mesh_buffers.h>
#include <lagrange/ui/utils/mesh.h>

#include <unordered_map>

namespace lagrange {
namespace ui {


void update_mesh_buffers_system(Registry& r)
{
    // Iterate over all MeshDataDirty
    {
        auto view = r.view<MeshDataDirty, MeshData, GLMesh>();

        for (auto e : view) {
            auto& mdd = view.get<MeshDataDirty>(e);
            if (mdd.all) {
                r.replace<GLMesh>(e, GLMesh{});
                continue;
            }

            if (mdd.vertices) {
                upload_mesh_vertices(
                    view.get<MeshData>(e),
                    *view.get<GLMesh>(e).attribute_buffers[DefaultShaderAtrribNames::Position]);
            }

            if (mdd.normals) {
                view.get<GLMesh>(e).attribute_buffers[DefaultShaderAtrribNames::Normal] = nullptr;
            }
        }
    }

    //Ensure attributes like normals/uvs/bitangents/tangents are present if shader needs them
    {
        auto view = r.view<MeshRender, MeshGeometry>();

        for (auto e : view) {
            const auto& mr = view.get<MeshRender>(e);
            const auto& mesh_entity = view.get<MeshGeometry>(e).entity;

            LA_ASSERT(r.valid(mesh_entity), "Invalid mesh entity " + ui::get_name(r,e));

            auto& md = r.get<MeshData>(mesh_entity);
            
            //No material set
            if (!mr.material) continue;

            auto shader_ref = ui::get_shader(r, mr.material->shader_id());
            if (!shader_ref) continue;

            auto& shader = *shader_ref;

            if (shader.attribs().find(DefaultShaderAtrribNames::Normal) != shader.attribs().end()) {
                ensure_normal(md);
            }

            if (shader.attribs().find(DefaultShaderAtrribNames::UV) != shader.attribs().end()) {
                ensure_uv(md);
            }

            if (shader.attribs().find(DefaultShaderAtrribNames::Tangent) !=
                    shader.attribs().end() ||
                shader.attribs().find(DefaultShaderAtrribNames::Bitangent) !=
                    shader.attribs().end()) {
                ensure_tangent_bitangent(md);
            }
        }
    }

    // Iterate over all MeshData
    // If they do not have GLMesh yet, create one
    // For each buffer, if it doesn't exist and the data is available, upload to GPU
    {
        auto view = r.view<MeshData>();
        for (auto e : view) {
            auto& meshdata = view.get<MeshData>(e);

            if (!r.has<GLMesh>(e)) r.emplace<GLMesh>(e);
            auto& glmesh = r.get<GLMesh>(e);


            if (!glmesh.get_attribute_buffer(DefaultShaderAtrribNames::Position)) {
                glmesh.attribute_buffers[DefaultShaderAtrribNames::Position] =
                    std::make_shared<GPUBuffer>();
                upload_mesh_vertices(
                    meshdata,
                    *glmesh.attribute_buffers[DefaultShaderAtrribNames::Position]);
            }

             if (!glmesh.get_index_buffer(DefaultShaderIndicesNames::TriangleIndices)) {
                glmesh.index_buffers[DefaultShaderIndicesNames::TriangleIndices] =
                    std::make_shared<GPUBuffer>(GL_ELEMENT_ARRAY_BUFFER);
                upload_mesh_triangles(
                    meshdata,
                    *glmesh.index_buffers[DefaultShaderIndicesNames::TriangleIndices]);
            }

            // Mapping between lagrange mesh and default shader attributes
            const static std::vector<std::pair<const char*, entt::id_type>> default_attribs = {
                {"normal", DefaultShaderAtrribNames::Normal},
                {"uv", DefaultShaderAtrribNames::UV},
                {"tangent", DefaultShaderAtrribNames::Tangent},
                {"bitangent", DefaultShaderAtrribNames::Bitangent},
                {"bone_ids", DefaultShaderAtrribNames::BoneIDs},
                {"bone_weights", DefaultShaderAtrribNames::BoneWeights},
            };

            for (const auto& attr : default_attribs) {
                // Already uploaded
                if (glmesh.get_attribute_buffer(attr.second)) continue;

                // If has corner attribute, upload
                if (has_mesh_corner_attribute(meshdata, attr.first)) {
                    glmesh.attribute_buffers[attr.second] = std::make_shared<GPUBuffer>();
                    upload_mesh_corner_attribute(
                        meshdata,
                        get_mesh_corner_attribute(meshdata, attr.first),
                        *glmesh.attribute_buffers[attr.second]);
                }
                // Otherwise try vertex attribute (and expand it to corner attribute for gpu upload)
                else if (has_mesh_vertex_attribute(meshdata, attr.first)) {
                    glmesh.attribute_buffers[attr.second] = std::make_shared<GPUBuffer>();
                    upload_mesh_vertex_attribute(
                        meshdata,
                        get_mesh_vertex_attribute(meshdata, attr.first),
                        *glmesh.attribute_buffers[attr.second]);
                }
            }

            // Material id 
            if (has_mesh_facet_attribute(meshdata, "material_id") &&
                glmesh.submesh_indices.size() == 0) {
                glmesh.submesh_indices = upload_submesh_indices(meshdata, "material_id");
            }
           
        }
    }
}


} // namespace ui
} // namespace lagrange

/*
 * Copyright 2021 Adobe. All rights reserved.
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

#include <lagrange/create_mesh.h>
#include <lagrange/ui/api.h>
#include <lagrange/ui/Entity.h>
#include <lagrange/ui/components/MeshData.h>
#include <lagrange/ui/types/Texture.h>
#include <lagrange/ui/utils/mesh.h>

#ifdef LAGRANGE_WITH_ASSIMP

#include <assimp/material.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>


namespace lagrange {
namespace ui {

namespace detail {

// TODO move to IO module
template <typename MeshType>
std::vector<Entity> load_meshes(Registry& r, const aiScene* scene)
{
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;
    using AttribArray = typename MeshType::AttributeArray;

    std::vector<Entity> meshes;

    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        const auto* amesh = scene->mMeshes[i];

        if (amesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE) {
            VertexArray vertices(amesh->mNumVertices, 3); // Triangulated input
            FacetArray faces(amesh->mNumFaces, 3);

            for (unsigned int j = 0; j < amesh->mNumVertices; ++j) {
                const aiVector3D& vec = amesh->mVertices[j];
                vertices(j, 0) = vec.x;
                vertices(j, 1) = vec.y;
                vertices(j, 2) = vec.z;
            }
            for (unsigned int j = 0; j < amesh->mNumFaces; ++j) {
                const aiFace& face = amesh->mFaces[j];
                assert(face.mNumIndices == 3);
                for (unsigned int k = 0; k < face.mNumIndices; ++k) {
                    faces(j, k) = face.mIndices[k];
                }
            }

            auto lgmesh = lagrange::create_mesh(std::move(vertices), std::move(faces));

            if (amesh->HasTextureCoords(0)) {
                typename MeshType::UVArray uvs(amesh->mNumVertices, 2);
                typename MeshType::UVIndices uv_indices = lgmesh->get_facets();

                assert(amesh->GetNumUVChannels() == 1); // assume one UV channel
                //assert(amesh->mNumUVComponents[0] == 2); // assume two components (uv), not 3d uvw
                for (unsigned int j = 0; j < amesh->mNumVertices; ++j) {
                    const aiVector3D& vec = amesh->mTextureCoords[0][j];
                    uvs(j, 0) = vec.x;
                    uvs(j, 1) = vec.y;
                }

                // uv indices are the same as facets
                lgmesh->initialize_uv(std::move(uvs), std::move(uv_indices));
                map_indexed_attribute_to_corner_attribute(*lgmesh, "uv");
            }


            // TODO bones
            if (amesh->HasBones()) {
            }

            if (amesh->HasTangentsAndBitangents()) {
                AttribArray tangents(amesh->mNumVertices, 3);
                AttribArray bitangents(amesh->mNumVertices, 3);

                for (unsigned int j = 0; j < amesh->mNumVertices; ++j) {
                    const aiVector3D& t = amesh->mTangents[j];
                    tangents(j, 0) = t.x;
                    tangents(j, 1) = t.y;
                    tangents(j, 2) = t.z;

                    const aiVector3D& bt = amesh->mBitangents[j];
                    bitangents(j, 0) = bt.x;
                    bitangents(j, 1) = bt.y;
                    bitangents(j, 2) = bt.z;
                }

                lgmesh->add_vertex_attribute("tangent");
                lgmesh->add_vertex_attribute("bitangent");
                lgmesh->import_vertex_attribute("tangent", tangents);
                lgmesh->import_vertex_attribute("bitangent", bitangents);
            }

            if (amesh->HasNormals()) {
                AttribArray normals(amesh->mNumVertices, 3);
                for (unsigned int j = 0; j < amesh->mNumVertices; ++j) {
                    const aiVector3D& t = amesh->mNormals[j];
                    normals(j, 0) = t.x;
                    normals(j, 1) = t.y;
                    normals(j, 2) = t.z;
                }
                lgmesh->add_vertex_attribute("normal");
                lgmesh->import_vertex_attribute("normal", normals);
            }
            meshes.push_back(register_mesh(r, std::move(lgmesh)));
        } else if (amesh->mPrimitiveTypes & aiPrimitiveType_POINT) {
            lagrange::logger().error("Point clouds not supported yet!");
            meshes.push_back(NullEntity);
        }
    }

    return meshes;
}

Entity load_scene_impl(
    Registry& r,
    const aiScene* scene,
    const fs::path& parent_path,
    const std::vector<Entity>& meshes);

} // namespace detail


template <typename MeshType>
Entity load_scene(
    Registry& r,
    const fs::path& path,
    unsigned int assimp_flags = aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace |
                                aiProcess_Triangulate | aiProcess_GenUVCoords)
{
    Assimp::Importer importer;

    try {
        const aiScene* scene = importer.ReadFile(
            path.string(),
            assimp_flags | aiProcess_Triangulate // Always triangulate
        );

        if (!scene) {
            logger().error("Error loading scene: {}", importer.GetErrorString());
            return NullEntity;
        }

    } catch (const std::exception& ex) {
        lagrange::logger().error("Exception in load_scene: {}", ex.what());
        return NullEntity;
    }

    std::unique_ptr<aiScene> ptr(importer.GetOrphanedScene());

    return detail::load_scene_impl(r, ptr.get(), path, detail::load_meshes<MeshType>(r, ptr.get()));
}


} // namespace ui
} // namespace lagrange

#endif

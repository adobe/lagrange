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

#ifdef LAGRANGE_WITH_ASSIMP

#include <lagrange/fs/filesystem.h>
#include <lagrange/create_mesh.h>
#include <lagrange/attributes/attribute_utils.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/Logger.h>
#include <lagrange/fs/file_utils.h>
#include <lagrange/legacy/inline.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/mesh.h>
#include <assimp/material.h>
#include <assimp/postprocess.h>

#include <string>
#include <iostream>

namespace lagrange::io {
LAGRANGE_LEGACY_INLINE
namespace legacy {

// ==== Declarations ====
std::unique_ptr<aiScene> load_scene_assimp(const lagrange::fs::path& filename);
std::unique_ptr<aiScene> load_scene_assimp_from_memory(const void* buffer, size_t size);

template <typename MeshType, 
    std::enable_if_t<lagrange::MeshTraitHelper::is_mesh<MeshType>::value>* = nullptr>
std::vector<std::unique_ptr<MeshType>> load_mesh_assimp(const lagrange::fs::path& filename);
template <typename MeshType>
std::vector<std::unique_ptr<MeshType>> load_mesh_assimp_from_memory(
    const void* buffer,
    size_t size);

template <typename MeshType>
std::vector<std::unique_ptr<MeshType>> extract_meshes_assimp(const aiScene* scene);
template <typename MeshType>
std::unique_ptr<MeshType> convert_mesh_assimp(const aiMesh* mesh);

inline std::unique_ptr<aiScene> load_scene_assimp(const lagrange::fs::path& filename)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filename.string(), 0);
    if (!scene) {
        logger().error("Error loading scene: {}", importer.GetErrorString());
        return nullptr;
    }

    return std::unique_ptr<aiScene>(importer.GetOrphanedScene());
}

inline std::unique_ptr<aiScene> load_scene_assimp_from_memory(const void* buffer, size_t size)
{
    Assimp::Importer importer;

    // Note: we are asking assimp to triangulate the scene for us.
    // Change this after we add support for n-gons.
    const aiScene* scene = importer.ReadFileFromMemory(buffer, size, aiProcess_Triangulate);

    if (!scene) {
        logger().error("Error loading scene: {}", importer.GetErrorString());
        return nullptr;
    }

    return std::unique_ptr<aiScene>(importer.GetOrphanedScene());
}

template <typename MeshType, 
    std::enable_if_t<lagrange::MeshTraitHelper::is_mesh<MeshType>::value>* /* = nullptr */>
std::vector<std::unique_ptr<MeshType>> load_mesh_assimp(const lagrange::fs::path& filename)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    std::unique_ptr<aiScene> scene = load_scene_assimp(filename);

    return extract_meshes_assimp<MeshType>(scene.get());
}

template <typename MeshType>
std::vector<std::unique_ptr<MeshType>> load_mesh_assimp_from_memory(const void* buffer, size_t size)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    std::unique_ptr<aiScene> scene = load_scene_assimp_from_memory(buffer, size);

    return extract_meshes_assimp<MeshType>(scene.get());
}

template <typename MeshType>
std::vector<std::unique_ptr<MeshType>> extract_meshes_assimp(const aiScene* scene)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    std::vector<std::unique_ptr<MeshType>> ret;
    if (!scene) {
        return ret;
    }
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        const aiMesh* mesh = scene->mMeshes[i];
        ret.emplace_back(convert_mesh_assimp<MeshType>(mesh));
    }
    return ret;
}

template <typename MeshType>
std::unique_ptr<MeshType> convert_mesh_assimp(const aiMesh* mesh)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;

    // Check that all facets have the same size
    unsigned int nvpf = 0;
    bool triangulate = false;
    for (unsigned int j = 0; j < mesh->mNumFaces; ++j) {
        const aiFace& face = mesh->mFaces[j];
        if (nvpf == 0) {
            nvpf = face.mNumIndices;
        } else if (face.mNumIndices != nvpf) {
            logger().warn("Facets with varying number of vertices detected, triangulating");
            nvpf = 3;
            triangulate = true;
            break;
        }
    }
    if (FacetArray::ColsAtCompileTime != Eigen::Dynamic && FacetArray::ColsAtCompileTime != nvpf) {
        logger().warn(
            "FacetArray cannot hold facets with n!={} vertex per facet, triangulating",
            FacetArray::ColsAtCompileTime);
        triangulate = true;
        nvpf = 3;
    }

    // If triangulating a heterogeneous mesh, we need to count the number of facets
    unsigned int num_output_facets = mesh->mNumFaces;
    if (triangulate) {
        num_output_facets = 0;
        for (unsigned int j = 0; j < mesh->mNumFaces; ++j) {
            const aiFace& face = mesh->mFaces[j];
            num_output_facets += face.mNumIndices - 2;
        }
    }

    VertexArray vertices(mesh->mNumVertices, 3); // should we support arbitrary dimension?
    for (unsigned int j = 0; j < mesh->mNumVertices; ++j) {
        const aiVector3D& vec = mesh->mVertices[j];
        vertices(j, 0) = vec.x;
        vertices(j, 1) = vec.y;
        vertices(j, 2) = vec.z;
    }

    FacetArray faces(num_output_facets, nvpf);
    for (unsigned int j = 0, f = 0; j < mesh->mNumFaces; ++j) {
        const aiFace& face = mesh->mFaces[j];
        if (triangulate) {
            for (unsigned int k = 2; k < face.mNumIndices; ++k) {
                faces.row(f++) << face.mIndices[0], face.mIndices[k - 1], face.mIndices[k];
            }
        } else {
            for (unsigned int k = 0; k < face.mNumIndices; ++k) {
                faces(j, k) = face.mIndices[k];
            }
        }
        assert(f <= num_output_facets);
    }

    auto lagrange_mesh = create_mesh(std::move(vertices), std::move(faces));

    if (mesh->HasTextureCoords(0)) {
        typename MeshType::UVArray uvs(mesh->mNumVertices, 2);
        typename MeshType::UVIndices uv_indices = lagrange_mesh->get_facets();

        assert(mesh->GetNumUVChannels() == 1); // assume one UV channel
        assert(mesh->mNumUVComponents[0] == 2); // assume two components (uv), not 3d uvw
        for (unsigned int j = 0; j < mesh->mNumVertices; ++j) {
            const aiVector3D& vec = mesh->mTextureCoords[0][j];
            uvs(j, 0) = vec.x;
            uvs(j, 1) = vec.y;
        }

        // uv indices are the same as facets

        lagrange_mesh->initialize_uv(std::move(uvs), std::move(uv_indices));
        map_indexed_attribute_to_corner_attribute(*lagrange_mesh, "uv");
    }

    return lagrange_mesh;
}

} // namespace legacy
} // namespace lagrange::io

#endif

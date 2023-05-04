/*
 * Copyright 2023 Adobe. All rights reserved.
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
#include <lagrange/scene/SimpleScene.h>
#include <lagrange/io/types.h>

#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <iosfwd>
#include <memory>

namespace lagrange::io::internal {
/**
 * Load an assimp scene from file.
 */
std::unique_ptr<aiScene> load_assimp(
    const fs::path& filename,
    unsigned int flags = aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace |
                         aiProcess_GenUVCoords | aiProcess_PopulateArmatureData);

/**
 * Load an assimp scene from stream.
 */
std::unique_ptr<aiScene> load_assimp(
    std::istream& input_stream,
    unsigned int flags = aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace |
                         aiProcess_GenUVCoords | aiProcess_PopulateArmatureData);

/**
 * Convert an assimp mesh to a lagrange SurfaceMesh
 */
template <typename MeshType>
MeshType convert_mesh_assimp_to_lagrange(const aiMesh& mesh, const LoadOptions& options = {});

/**
 * Load a mesh using assimp. If the scene contains multiple meshes, they will be combined into one.
 */
template <typename MeshType>
MeshType load_mesh_assimp(const aiScene& scene, const LoadOptions& options = {});

/**
 * Load a simple scene with assimp.
 */
template <typename SceneType>
SceneType load_simple_scene_assimp(const aiScene& scene, const LoadOptions& options = {});

}

#endif

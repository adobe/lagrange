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

#include <lagrange/SurfaceMesh.h>
#include <lagrange/fs/filesystem.h>
#include <lagrange/io/types.h>
#include <lagrange/scene/SimpleScene.h>
#include <lagrange/utils/assert.h>
#include <tiny_gltf.h>

namespace lagrange::io::internal {

/**
 * Load a gltf model.
 */
tinygltf::Model load_tinygltf(const fs::path& filename);

/**
 * Load a gltf model.
 */
tinygltf::Model load_tinygltf(std::istream& input_stream);

/**
 * Convert a gltf primitive to a lagrange SurfaceMesh
 */
template <typename MeshType>
MeshType convert_tinygltf_primitive_to_lagrange_mesh(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    const LoadOptions& options = {});

/**
 * Load a mesh using gltf. If the scene contains multiple meshes, they will be merged into one.
 */
template <typename MeshType>
MeshType load_mesh_gltf(const tinygltf::Model& model, const LoadOptions& options = {});

/**
 * Load a simple scene with gltf.
 */
template <typename SceneType>
SceneType load_simple_scene_gltf(const tinygltf::Model& model, const LoadOptions& options = {});

/**
 * Load a scene with gltf.
 */
template <typename SceneType>
SceneType load_scene_gltf(const tinygltf::Model& model, const LoadOptions& options = {});

} // namespace lagrange::io::internal

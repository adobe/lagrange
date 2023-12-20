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
#include <lagrange/io/types.h>

#include <ufbx.h>

namespace lagrange::io::internal {

/**
 * Load an ufbx scene from file.
 * 
 * Don't forget to call ufbx_free_scene after.
 */
ufbx_scene* load_ufbx(const fs::path& filename);

/**
 * Load an ufbx scene from stream.
 * 
 * Don't forget to call ufbx_free_scene after.
 */
ufbx_scene* load_ufbx(std::istream& input_stream);

/**
 * Convert an ufbx mesh into a lagrange SurfaceMesh.
 */
template <typename MeshType>
MeshType convert_mesh_ufbx_to_lagrange(const ufbx_mesh* mesh, const LoadOptions& opt);

/**
 * Load a mesh using ufbx. If the scene contains multiple meshes, they will be merged into one.
 */
template <typename MeshType>
MeshType load_mesh_fbx(const ufbx_scene* scene, const LoadOptions& options = {});

/**
 * Load a simple scene with ufbx.
 */
template <typename SceneType>
SceneType load_simple_scene_fbx(const ufbx_scene* scene, const LoadOptions& options = {});

/**
 * Load a scene with ufbx.
 */
template <typename SceneType>
SceneType load_scene_fbx(const ufbx_scene* scene, const LoadOptions& options = {});

} // namespace lagrange::io::internal

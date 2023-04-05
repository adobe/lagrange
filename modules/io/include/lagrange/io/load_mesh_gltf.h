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

#include <iosfwd>

namespace lagrange::io {

/**
 * Loads a mesh from an input stream in glTF or GLB format. If the scene contains multiple meshes,
 * they will be merged into one.
 *
 * @param input_stream   Input data stream.
 * @param options        Load options.
 *
 * @tparam MeshType      Mesh type to load.
 *
 * @return               Loaded mesh.
 */
template <typename MeshType>
MeshType load_mesh_gltf(std::istream& input_stream, const LoadOptions& options = {});

/**
 * Loads a mesh from a file in glTF or GLB format. If the scene contains multiple meshes, they will
 * be merged into one.
 *
 * @param[in]  filename  Input filename.
 * @param[in]  options   Load options.
 *
 * @tparam     MeshType  Mesh type to load.
 *
 * @return     Loaded mesh.
 */
template <typename MeshType>
MeshType load_mesh_gltf(const fs::path& filename, const LoadOptions& options = {});

} // namespace lagrange::io

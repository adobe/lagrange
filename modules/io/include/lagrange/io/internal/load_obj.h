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

#include <tiny_obj_loader.h>

#include <iosfwd>

namespace lagrange::io::internal {

/**
 * Load with tinyobj from file.
 */
tinyobj::ObjReader load_obj(const fs::path& filename, const LoadOptions& options = {});

/**
 * Load with tinyobj from stream.
 */
tinyobj::ObjReader load_obj(
    std::istream& input_stream_obj,
    std::istream& input_stream_mtl,
    const LoadOptions& options = {});

/**
 * Load a mesh using obj.
 */
template <typename MeshType>
MeshType load_mesh_obj(const tinyobj::ObjReader& reader, const LoadOptions& options = {});

/**
 * Load a simple scene using obj.
 */
template <typename SceneType>
SceneType load_simple_scene_obj(const tinyobj::ObjReader& reader, const LoadOptions& options = {});

/**
 * Load a scene using obj.
 */
template <typename SceneType>
SceneType load_scene_obj(const tinyobj::ObjReader& reader, const LoadOptions& options = {});


// ==== Old API below ====

/**
 * Output of the obj mesh loader
 *
 * @tparam     Scalar  Mesh scalar type
 * @tparam     Index   Mesh index type
 */
template <typename Scalar, typename Index>
struct ObjReaderResult
{
    /// Whether the load operation was successful.
    bool success = true;

    /// Aggregated mesh containing all elements in the .obj file. To separate the different
    /// entities, split the mesh facets based on object ids.
    SurfaceMesh<Scalar, Index> mesh;

    /// Materials associated with the mesh.
    std::vector<tinyobj::material_t> materials;

    /// Names of each object in the aggregate mesh.
    std::vector<std::string> names;
};

template <typename MeshType>
ObjReaderResult<typename MeshType::Scalar, typename MeshType::Index> extract_mesh(
    const tinyobj::ObjReader& reader,
    const LoadOptions& options = {});

/**
 * Loads a .obj mesh from a file.
 *
 * @param[in]  filename  The file to read data from.
 * @param[in]  options   Optional configuration for the loader.
 *
 * @tparam     MeshType  Mesh type to load.
 *
 * @return     Result of the load.
 */
template <typename MeshType>
auto load_mesh_obj(const fs::path& filename, const LoadOptions& options = {})
    -> ObjReaderResult<typename MeshType::Scalar, typename MeshType::Index>;

/**
 * Loads a .obj mesh from a file.
 *
 * @param[in]  input_stream The stream to read data from.
 * @param[in]  options      Optional configuration for the loader.
 *
 * @tparam     MeshType     Mesh type to load.
 *
 * @return     Result of the load.
 */
template <typename MeshType>
auto load_mesh_obj(
    std::istream& input_stream_obj,
    std::istream& input_stream_mtl,
    const LoadOptions& options = {})
    -> ObjReaderResult<typename MeshType::Scalar, typename MeshType::Index>;

} // namespace lagrange::io::internal

/*
 * Copyright 2022 Adobe. All rights reserved.
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

// TODO: Hide in .cpp
#include <tiny_obj_loader.h>

#include <istream>
#include <vector>

namespace lagrange {
namespace io {

///
/// Config options for the obj mesh loader.
///
struct ObjReaderOptions
{
    /// Triangulate any polygonal facet with > 3 vertices
    bool triangulate = false;

    /// Load vertex normals as indexed attributes
    bool load_normals = true;

    /// Load texture coordinates as indexed attributes
    bool load_uvs = true;

    /// Load material ids as facet attributes
    bool load_materials = true;

    /// Load vertex colors as vertex attributes
    bool load_vertex_colors = false;

    /// Load object id as facet attributes
    bool load_object_id = true;

    /// Search path for .mtl files. By default, searches the same folder as the provided filename.
    std::string mtl_search_path;
};

///
/// Output of the obj mesh loader.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
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

///
/// Loads a .obj mesh from a stream.
///
/// @param[in]  filename  The stream to read data from.
/// @param[in]  options   Optional configuration for the loader.
///
/// @tparam     MeshType  Mesh type to load.
///
/// @return     Result of the load.
///
template <typename MeshType>
auto load_mesh_obj(const std::string& filename, const ObjReaderOptions& options = {})
    -> ObjReaderResult<typename MeshType::Scalar, typename MeshType::Index>;

} // namespace io
} // namespace lagrange

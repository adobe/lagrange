/*
 * Copyright 2026 Adobe. All rights reserved.
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
#include <lagrange/io/api.h>
#include <lagrange/io/types.h>

#include <iosfwd>

namespace lagrange::io {

/**
 * Loads a point cloud from a stream in PCD (Point Cloud Data) format.
 *
 * The resulting mesh contains only vertices (no facets). The `x`, `y`, `z` fields populate the
 * vertex positions. `normal_x`/`normal_y`/`normal_z` (or `nx`/`ny`/`nz`) are loaded as a
 * `AttributeName::normal` vertex attribute. Packed `rgb`/`rgba` fields are unpacked into a
 * `AttributeName::color` vertex attribute (uint8, 3 or 4 channels). Any remaining field is loaded
 * as a vertex attribute named after the field. The `ascii`, `binary`, and `binary_compressed`
 * (LZF) PCD data sections are all supported.
 *
 * @param[in]  input_stream  Input stream.
 * @param[in]  options       Load options.
 *
 * @tparam     MeshType      Mesh type to load.
 *
 * @return     Loaded point cloud.
 */
template <typename MeshType>
LA_IO_API MeshType load_mesh_pcd(std::istream& input_stream, const LoadOptions& options = {});

/**
 * @overload
 *
 * Loads a point cloud from a file in PCD (Point Cloud Data) format.
 *
 * @param[in]  filename  Input filename.
 * @param[in]  options   Load options.
 *
 * @tparam     MeshType  Mesh type to load.
 *
 * @see        @ref load_mesh_pcd
 *
 * @return     Loaded point cloud.
 */
template <typename MeshType>
LA_IO_API MeshType load_mesh_pcd(const fs::path& filename, const LoadOptions& options = {});

} // namespace lagrange::io

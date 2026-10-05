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

///
/// Saves a mesh (point cloud) to a stream in PCD (Point Cloud Data) format.
///
/// Only vertices are written (facets are ignored). Vertex positions are written as `x`/`y`/`z`
/// fields. A vertex attribute with `AttributeUsage::Normal` is written as
/// `normal_x`/`normal_y`/`normal_z`. A vertex attribute with `AttributeUsage::Color` is written as
/// a packed `rgb` (3 channels) or `rgba` (4 channels) field. Any other vertex attribute is written
/// as a field named after the attribute. When `encoding` is `Ascii` the data is written as `ascii`;
/// otherwise it is written as LZF-compressed `binary_compressed` (binary output is always
/// compressed).
///
/// @param[in,out] output_stream  Output stream.
/// @param[in]     mesh           Mesh to write.
/// @param[in]     options        Save options.
///
/// @tparam        Scalar         Mesh scalar type.
/// @tparam        Index          Mesh index type.
///
template <typename Scalar, typename Index>
LA_IO_API void save_mesh_pcd(
    std::ostream& output_stream,
    const SurfaceMesh<Scalar, Index>& mesh,
    const SaveOptions& options = {});

///
/// @overload
///
/// Saves a mesh (point cloud) to a file in PCD (Point Cloud Data) format.
///
/// @param[in]  filename  Output filename.
/// @param[in]  mesh      Mesh to write.
/// @param[in]  options   Save options.
///
/// @tparam     Scalar    Mesh scalar type.
/// @tparam     Index     Mesh index type.
///
template <typename Scalar, typename Index>
LA_IO_API void save_mesh_pcd(
    const fs::path& filename,
    const SurfaceMesh<Scalar, Index>& mesh,
    const SaveOptions& options = {});

} // namespace lagrange::io

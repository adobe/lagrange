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
 * Saves a mesh to an output stream in glTF or GLB format.
 *
 * @param[in]  output_stream Output data stream.
 * @param[in]  mesh          Mesh to write.
 * @param[in]  options       Save options.
 *
 * @tparam     Scalar        Mesh scalar type.
 * @tparam     Index         Mesh index type.
 */
template <typename Scalar, typename Index>
void save_mesh_gltf(
    std::ostream& output_stream,
    const SurfaceMesh<Scalar, Index>& mesh,
    const SaveOptions& options = {});

/**
 * Saves a mesh to a file in glTF or GLB format.
 *
 * @param[in]  filename  Output filename.
 * @param[in]  mesh      Mesh to write.
 * @param[in]  options   Save options.
 *
 * @tparam     Scalar    Mesh scalar type.
 * @tparam     Index     Mesh index type.
 */
template <typename Scalar, typename Index>
void save_mesh_gltf(
    const fs::path& filename,
    const SurfaceMesh<Scalar, Index>& mesh,
    const SaveOptions& options = {});

} // namespace lagrange::io

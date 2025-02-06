/*
 * Copyright 2024 Adobe. All rights reserved.
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
 * Loads a mesh from a stream in STL format.
 *
 * @param[in]  input_stream  Input stream.
 * @param[in]  options       Load options.
 *
 * @return     Loaded mesh.
 */
template <typename MeshType>
MeshType load_mesh_stl(std::istream& input_stream, const LoadOptions& options = {});

/**
 * @overload
 *
 * Loads a mesh from a file in STL format.
 *
 * @param[in]  filename  Input filename.
 * @param[in]  options   Load options.
 *
 * @see        load_mesh_stl(std::istream&, const LoadOptions&) for more info.
 *
 * @return     Loaded mesh.
 */
template <typename MeshType>
MeshType load_mesh_stl(const fs::path& filename, const LoadOptions& options = {});

} // namespace lagrange::io

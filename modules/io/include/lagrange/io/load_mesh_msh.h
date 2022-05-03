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
#include <lagrange/fs/filesystem.h>

namespace lagrange::io {

/**
 * Load a .msh file.
 *
 * @param[in]  input_stream  Input stream
 *
 * @return  The loaded surface mesh.
 */
template <typename MeshType>
MeshType load_mesh_msh(std::istream& input_stream);

/**
 * @overload
 *
 * @param[in]  filename  Input mesh filename.
 *
 * @see load_mesh_msh(std::istream&) for more info.
 */
template <typename MeshType>
MeshType load_mesh_msh(const fs::path& filename) {
    fs::ifstream fin(filename);
    return load_mesh_msh<MeshType>(fin);
}

} // namespace lagrange::io

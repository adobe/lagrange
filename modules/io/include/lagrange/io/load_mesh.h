/*
 * Copyright 2017 Adobe. All rights reserved.
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
#include <lagrange/MeshTrait.h>
#include <lagrange/io/types.h>

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
#include <lagrange/io/legacy/load_mesh.h>
#endif

namespace lagrange::io {

// The input stream version of load_mesh does not exist because it cannot determine the file type.
// If you want to load a mesh from an input stream, use a specific `load_mesh_type` function.

/**
 * Load a mesh from a file. The loader will be chosen depending on the file extension.
 *
 * @param[in]  filename  Input file name.
 * @param[in]  options   Extra options related to loading.
 *
 * @return A `SurfaceMesh` object loaded from the input file.
 */
template <
    typename MeshType,
    std::enable_if_t<!lagrange::MeshTraitHelper::is_mesh<MeshType>::value>* = nullptr>
MeshType load_mesh(const fs::path& filename, const LoadOptions& = {});

} // namespace lagrange::io

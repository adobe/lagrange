/*
 * Copyright 2021 Adobe. All rights reserved.
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

#ifdef LAGRANGE_WITH_ASSIMP

#include <lagrange/SurfaceMesh.h>
#include <lagrange/fs/filesystem.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/io/types.h>

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/io/legacy/load_mesh_assimp.h>
#endif

namespace lagrange::io {

/**
 * Load a mesh using assimp. If the scene contains multiple meshes, they will be combined into one.
 *
 * @param[in] filename input filename
 * @param[in] options
 *
 * @return loaded mesh
 */
template <typename MeshType, 
    std::enable_if_t<!lagrange::MeshTraitHelper::is_mesh<MeshType>::value>* = nullptr>
MeshType load_mesh_assimp(const fs::path& filename, const LoadOptions& options = {});

} // namespace lagrange::io

#endif

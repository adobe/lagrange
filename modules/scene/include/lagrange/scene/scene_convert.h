/*
 * Copyright 2025 Adobe. All rights reserved.
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
#include <lagrange/scene/Scene.h>
#include <lagrange/types/TransformOptions.h>

#include <vector>

// TODO: We're still missing scene <-> simple scene conversion functions.
namespace lagrange::scene {

///
/// Converts a single mesh into a scene with a single identity instance of the input mesh.
///
/// @param[in]  mesh    Input mesh to convert.
///
/// @tparam     Scalar  Input mesh scalar type.
/// @tparam     Index   Input mesh index type.
///
/// @return     Scene containing the input mesh.
///
template <typename Scalar, typename Index>
Scene<Scalar, Index> mesh_to_scene(SurfaceMesh<Scalar, Index> mesh);

///
/// Converts a list of meshes into a scene with a single identity instance of each input mesh.
///
/// @param[in]  meshes  Input meshes to convert.
///
/// @tparam     Scalar  Input mesh scalar type.
/// @tparam     Index   Input mesh index type.
///
/// @return     Scene containing the input meshes.
///
template <typename Scalar, typename Index>
Scene<Scalar, Index> meshes_to_scene(std::vector<SurfaceMesh<Scalar, Index>> meshes);

///
/// Converts a scene into a concatenated mesh with all the transforms applied.
///
/// @param[in]  scene                Scene to convert.
/// @param[in]  transform_options    Options to use when applying mesh transformations.
/// @param[in]  preserve_attributes  Preserve shared attributes and map them to the output mesh.
///
/// @tparam     Scalar               Input scene scalar type.
/// @tparam     Index                Input scene index type.
///
/// @return     Concatenated mesh.
///
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> scene_to_mesh(
    const Scene<Scalar, Index>& scene,
    const TransformOptions& transform_options = {},
    bool preserve_attributes = true);

} // namespace lagrange::scene

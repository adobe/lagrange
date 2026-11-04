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
#include <lagrange/scene/SimpleScene.h>
#include <lagrange/types/TransformOptions.h>

#include <vector>

namespace lagrange::scene {

///
/// @addtogroup module-scene
/// @{
///

///
/// Converts a single mesh into a simple scene with a single identity instance of the input mesh.
///
/// @param[in]  mesh       Input mesh, whose dimension must equal @p Dimension.
///
/// @tparam     Dimension  Output scene dimension.
/// @tparam     Scalar     Input mesh scalar type.
/// @tparam     Index      Input mesh index type.
///
/// @return     Simple scene containing the input mesh.
///
template <size_t Dimension = 3, typename Scalar, typename Index>
SimpleScene<Scalar, Index, Dimension> mesh_to_simple_scene(SurfaceMesh<Scalar, Index> mesh);

///
/// Converts a list of meshes into a simple scene with a single identity instance of each input
/// mesh.
///
/// @param[in]  meshes     Input meshes, each of dimension @p Dimension, retained in input order.
///
/// @tparam     Dimension  Output scene dimension.
/// @tparam     Scalar     Input mesh scalar type.
/// @tparam     Index      Input mesh index type.
///
/// @return     Simple scene containing the input meshes.
///
template <size_t Dimension = 3, typename Scalar, typename Index>
SimpleScene<Scalar, Index, Dimension> meshes_to_simple_scene(
    std::vector<SurfaceMesh<Scalar, Index>> meshes);

///
/// Concatenates all mesh instances with their transforms applied. Meshes without instances are
/// omitted; each instance contributes a separate copy of its source mesh.
///
/// @param[in]  scene                Scene to convert.
/// @param[in]  transform_options    Options to use when applying mesh transformations.
/// @param[in]  preserve_attributes  Preserve shared attributes and map them to the output mesh.
///
/// @tparam     Scalar               Input scene scalar type.
/// @tparam     Index                Input scene index type.
/// @tparam     Dimension            Input scene dimension.
///
/// @return     Concatenated mesh.
///
template <typename Scalar, typename Index, size_t Dimension>
SurfaceMesh<Scalar, Index> simple_scene_to_mesh(
    const SimpleScene<Scalar, Index, Dimension>& scene,
    const TransformOptions& transform_options = {},
    bool preserve_attributes = true);

///
/// Converts each mesh instance into a mesh with its transform applied. Meshes without instances
/// are omitted. The input scene is not modified.
///
/// @param[in]  scene              Scene to convert.
/// @param[in]  transform_options  Options to use when applying mesh transformations.
///
/// @tparam     Scalar             Input scene scalar type.
/// @tparam     Index              Input scene index type.
/// @tparam     Dimension          Input scene dimension.
///
/// @return     One transformed mesh per instance, ordered by source mesh index, then local
///             instance index. Instance user data is not included.
///
template <typename Scalar, typename Index, size_t Dimension>
std::vector<SurfaceMesh<Scalar, Index>> simple_scene_to_meshes(
    const SimpleScene<Scalar, Index, Dimension>& scene,
    const TransformOptions& transform_options = {});

/// @}

} // namespace lagrange::scene

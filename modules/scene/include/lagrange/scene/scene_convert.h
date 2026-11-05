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
#include <lagrange/scene/SimpleScene.h>
#include <lagrange/types/TransformOptions.h>

#include <vector>

namespace lagrange::scene {

///
/// @addtogroup module-scene
/// @{
///

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
/// Only mesh instances are included; uninstantiated meshes are omitted. Instance transforms are
/// accumulated along each node's parent chain. See scene_to_meshes_and_materials() for material
/// attribute conventions.
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

///
/// Converts a scene into a list of meshes with all the transforms applied.
///
/// Outputs follow node index order, then instance order within each node. All nodes are visited,
/// regardless of membership in `Scene::root_nodes`; meshes without instances are omitted.
/// See scene_to_meshes_and_materials() for material attribute conventions.
///
/// @param[in]  scene              Scene to convert.
/// @param[in]  transform_options  Options to use when applying mesh transformations.
///
/// @tparam     Scalar             Input scene scalar type.
/// @tparam     Index              Input scene index type.
///
/// @return     List of meshes with transforms applied. Each mesh carries a `material_id` facet
///             attribute (see @ref AttributeName::material_id) whose values are global indices into
///             `Scene::materials`. Facets with no associated material are marked with
///             `invalid<Index>()`.
///
template <typename Scalar, typename Index>
std::vector<SurfaceMesh<Scalar, Index>> scene_to_meshes(
    const Scene<Scalar, Index>& scene,
    const TransformOptions& transform_options = {});

///
/// Converts a Scene into a SimpleScene.
///
/// Each mesh instance in every node becomes a MeshInstance with the accumulated world transform.
/// All source meshes are copied in index order, including meshes without instances. Materials and
/// other scene metadata (images, textures, cameras, lights) are not preserved. Mesh attributes are
/// copied unchanged, including any instance-local `material_id` values.
///
/// @param[in]  scene   Input scene to convert.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
/// @return     SimpleScene containing all mesh instances from the scene.
///
template <typename Scalar, typename Index>
SimpleScene<Scalar, Index> scene_to_simple_scene(const Scene<Scalar, Index>& scene);

///
/// Converts a SimpleScene into a Scene.
///
/// Each mesh instance becomes a node with the instance transform, converted to single precision.
/// All instance nodes are direct children of a single root node. All source meshes are copied in
/// index order, including meshes without instances. Instance user data is not preserved, and no
/// scene materials are created.
///
/// @param[in]  simple_scene  Input simple scene to convert.
///
/// @tparam     Scalar        Mesh scalar type.
/// @tparam     Index         Mesh index type.
///
/// @return     Scene containing the meshes and instances from the SimpleScene.
///
template <typename Scalar, typename Index>
Scene<Scalar, Index> simple_scene_to_scene(const SimpleScene<Scalar, Index>& simple_scene);

///
/// Result structure for scene to meshes and materials conversion.
///
/// @tparam     Scalar             Mesh scalar type.
/// @tparam     Index              Mesh index type.
///
template <typename Scalar, typename Index>
struct MeshesAndMaterialsResult
{
    /// List of meshes with global transforms applied, one per scene mesh instance. Each mesh carries
    /// a `material_id` facet attribute (see @ref AttributeName::material_id) whose values are global
    /// indices into `Scene::materials`, not local indices into `material_ids[i]`.
    /// Facets with no associated material are marked with `invalid<Index>()`.
    std::vector<SurfaceMesh<Scalar, Index>> meshes;

    /// Material IDs into `Scene::materials`, one list per mesh instance and aligned with `meshes`.
    /// `material_ids[i]` is an unchanged copy of `SceneMeshInstance::materials` for `meshes[i]`.
    /// Material objects are not returned. Lists are not deduplicated or validated and may contain
    /// invalid IDs or unused materials. Use each mesh's `material_id` facet attribute for exact assignments.
    std::vector<std::vector<ElementId>> material_ids;
};

///
/// Converts a scene into a list of meshes with all the transforms applied and a list of material
/// IDs.
///
/// In addition to the per-mesh `material_ids` list, every output mesh is given a `material_id`
/// facet attribute holding the global scene material index used by each facet, so that the
/// facet-to-material assignment is preserved even when a single mesh uses multiple materials.
///
/// Outputs follow node index order, then instance order within each node. All nodes are visited,
/// regardless of membership in `Scene::root_nodes`; meshes without instances are omitted.
///
/// **Convention for existing `material_id` attributes:** If an input mesh already carries a
/// `material_id` facet attribute, its values are interpreted as *instance-local* indices into the
/// corresponding `SceneMeshInstance::materials` list. Because instances are flattened and meshes
/// are duplicated, this function remaps them to global `Scene::materials` indices on output.
///
/// The existing attribute must have one channel and be facet-associated; its values are cast to
/// `Index`. Invalid local slots or scene material indices become `invalid<Index>()`. If no
/// attribute exists, every facet uses the first listed material, or `invalid<Index>()` if no valid
/// first material is available.
///
/// @param[in]  scene              Scene to convert.
/// @param[in]  transform_options  Options to use when applying mesh transformations.
///
/// @tparam     Scalar             Input scene scalar type.
/// @tparam     Index              Input scene index type.
///
/// @return     A result containing one globally transformed mesh per scene mesh instance and a
///             parallel list of material-ID lists. `material_ids[i]` is the instance's unchanged
///             `SceneMeshInstance::materials` list for `meshes[i]`, with IDs referring to
///             `Scene::materials`; lists are not deduplicated or validated.
///
template <typename Scalar, typename Index>
MeshesAndMaterialsResult<Scalar, Index> scene_to_meshes_and_materials(
    const Scene<Scalar, Index>& scene,
    const TransformOptions& transform_options = {});

/// @}

} // namespace lagrange::scene

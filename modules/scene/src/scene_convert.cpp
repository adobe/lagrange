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
#include <lagrange/scene/scene_convert.h>

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/combine_meshes.h>
#include <lagrange/scene/scene_utils.h>
#include <lagrange/transform_mesh.h>
#include <lagrange/utils/assert.h>

namespace lagrange::scene {

template <typename Scalar, typename Index>
Scene<Scalar, Index> mesh_to_scene(SurfaceMesh<Scalar, Index> mesh)
{
    return meshes_to_scene<Scalar, Index>({std::move(mesh)});
}

template <typename Scalar, typename Index>
Scene<Scalar, Index> meshes_to_scene(std::vector<SurfaceMesh<Scalar, Index>> meshes)
{
    Scene<Scalar, Index> scene;
    scene.meshes.reserve(meshes.size());
    scene::Node node;
    for (auto& mesh : meshes) {
        ElementId mesh_idx = scene.add(std::move(mesh));
        node.meshes.push_back({mesh_idx, {}});
    }
    scene.nodes.push_back(node);
    scene.root_nodes.push_back(0);
    return scene;
}

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> scene_to_mesh(
    const Scene<Scalar, Index>& scene,
    const TransformOptions& transform_options,
    bool preserve_attributes)
{
    std::vector<SurfaceMesh<Scalar, Index>> meshes;

    for (ElementId node_id = 0; node_id < scene.nodes.size(); ++node_id) {
        const auto& node = scene.nodes[node_id];
        if (node.meshes.empty()) {
            continue;
        }
        auto world_from_mesh =
            utils::compute_global_node_transform(scene, node_id).template cast<Scalar>();
        for (const SceneMeshInstance& mesh_instance : node.meshes) {
            const auto mesh_id = mesh_instance.mesh;
            meshes.emplace_back(
                transformed_mesh<Scalar, Index>(
                    scene.meshes.at(mesh_id),
                    world_from_mesh,
                    transform_options));
        }
    }

    return combine_meshes<Scalar, Index>(meshes, preserve_attributes);
}

#define LA_X_scene_convert(_, Scalar, Index)                                                   \
    template LA_SCENE_API Scene<Scalar, Index> mesh_to_scene(SurfaceMesh<Scalar, Index> mesh); \
    template LA_SCENE_API Scene<Scalar, Index> meshes_to_scene(                                \
        std::vector<SurfaceMesh<Scalar, Index>> meshes);                                       \
    template LA_SCENE_API SurfaceMesh<Scalar, Index> scene_to_mesh(                            \
        const Scene<Scalar, Index>& scene,                                                     \
        const TransformOptions& transform_options,                                             \
        bool preserve_attributes);
LA_SURFACE_MESH_X(scene_convert, 0)

} // namespace lagrange::scene

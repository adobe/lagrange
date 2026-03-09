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
#include <lagrange/scene/SimpleScene.h>
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
MeshesAndMaterialsResult<Scalar, Index> scene_to_meshes_and_materials(
    const Scene<Scalar, Index>& scene,
    const TransformOptions& transform_options)
{
    MeshesAndMaterialsResult<Scalar, Index> ret;

    for (ElementId node_id = 0; node_id < scene.nodes.size(); ++node_id) {
        const auto& node = scene.nodes[node_id];
        if (node.meshes.empty()) {
            continue;
        }
        auto world_from_mesh =
            utils::compute_global_node_transform(scene, node_id).template cast<Scalar>();
        for (const SceneMeshInstance& mesh_instance : node.meshes) {
            const auto mesh_id = mesh_instance.mesh;
            ret.meshes.emplace_back(
                transformed_mesh<Scalar, Index>(
                    scene.meshes.at(mesh_id),
                    world_from_mesh,
                    transform_options));
            ret.material_ids.push_back(mesh_instance.materials);
        }
    }

    return ret;
}

template <typename Scalar, typename Index>
std::vector<SurfaceMesh<Scalar, Index>> scene_to_meshes(
    const Scene<Scalar, Index>& scene,
    const TransformOptions& transform_options)
{
    return scene_to_meshes_and_materials(scene, transform_options).meshes;
}

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> scene_to_mesh(
    const Scene<Scalar, Index>& scene,
    const TransformOptions& transform_options,
    bool preserve_attributes)
{
    return combine_meshes<Scalar, Index>(
        scene_to_meshes(scene, transform_options),
        preserve_attributes);
}

template <typename Scalar, typename Index>
SimpleScene<Scalar, Index> scene_to_simple_scene(const Scene<Scalar, Index>& scene)
{
    SimpleScene<Scalar, Index> simple_scene;

    // Copy meshes.
    simple_scene.reserve_meshes(static_cast<Index>(scene.meshes.size()));
    for (size_t i = 0; i < scene.meshes.size(); ++i) {
        simple_scene.add_mesh(scene.meshes[i]);
    }

    // Flatten the node hierarchy into mesh instances with world transforms.
    for (ElementId node_id = 0; node_id < scene.nodes.size(); ++node_id) {
        const auto& node = scene.nodes[node_id];
        if (node.meshes.empty()) continue;

        auto world_transform = utils::compute_global_node_transform(scene, node_id);

        for (const SceneMeshInstance& mi : node.meshes) {
            using InstanceType = MeshInstance<Scalar, Index>;
            InstanceType instance;
            instance.mesh_index = static_cast<Index>(mi.mesh);
            instance.transform = world_transform.template cast<Scalar>();
            simple_scene.add_instance(std::move(instance));
        }
    }

    return simple_scene;
}

template <typename Scalar, typename Index>
Scene<Scalar, Index> simple_scene_to_scene(const SimpleScene<Scalar, Index>& simple_scene)
{
    Scene<Scalar, Index> scene;

    // Copy meshes.
    scene.meshes.reserve(simple_scene.get_num_meshes());
    for (Index i = 0; i < simple_scene.get_num_meshes(); ++i) {
        scene.meshes.push_back(simple_scene.get_mesh(i));
    }

    // Create a root node.
    Node root;
    root.name = "root";
    ElementId root_id = scene.add(std::move(root));
    scene.root_nodes.push_back(root_id);

    // Create one child node per mesh instance.
    for (Index mesh_idx = 0; mesh_idx < simple_scene.get_num_meshes(); ++mesh_idx) {
        for (Index inst_idx = 0; inst_idx < simple_scene.get_num_instances(mesh_idx); ++inst_idx) {
            const auto& instance = simple_scene.get_instance(mesh_idx, inst_idx);
            la_debug_assert(instance.mesh_index == mesh_idx);

            Node child;
            child.transform = instance.transform.template cast<float>();
            child.parent = root_id;

            SceneMeshInstance mi;
            mi.mesh = mesh_idx;
            child.meshes.push_back(std::move(mi));

            ElementId child_id = scene.add(std::move(child));
            scene.add_child(root_id, child_id);
        }
    }

    return scene;
}

#define LA_X_scene_convert(_, Scalar, Index)                                                     \
    template LA_SCENE_API Scene<Scalar, Index> mesh_to_scene(SurfaceMesh<Scalar, Index> mesh);   \
    template LA_SCENE_API Scene<Scalar, Index> meshes_to_scene(                                  \
        std::vector<SurfaceMesh<Scalar, Index>> meshes);                                         \
    template LA_SCENE_API SurfaceMesh<Scalar, Index> scene_to_mesh(                              \
        const Scene<Scalar, Index>& scene,                                                       \
        const TransformOptions& transform_options,                                               \
        bool preserve_attributes);                                                               \
    template LA_SCENE_API std::vector<SurfaceMesh<Scalar, Index>> scene_to_meshes(               \
        const Scene<Scalar, Index>& scene,                                                       \
        const TransformOptions& transform_options);                                              \
    template LA_SCENE_API SimpleScene<Scalar, Index> scene_to_simple_scene(                      \
        const Scene<Scalar, Index>& scene);                                                      \
    template LA_SCENE_API Scene<Scalar, Index> simple_scene_to_scene(                            \
        const SimpleScene<Scalar, Index>& simple_scene);                                         \
    template LA_SCENE_API MeshesAndMaterialsResult<Scalar, Index> scene_to_meshes_and_materials( \
        const Scene<Scalar, Index>& scene,                                                       \
        const TransformOptions& transform_options);
LA_SURFACE_MESH_X(scene_convert, 0)

} // namespace lagrange::scene

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
#include <lagrange/scene/simple_scene_convert.h>

#include <lagrange/combine_meshes.h>
#include <lagrange/scene/SimpleSceneTypes.h>
#include <lagrange/transform_mesh.h>
#include <lagrange/utils/assert.h>

namespace lagrange::scene {

template <size_t Dimension, typename Scalar, typename Index>
SimpleScene<Scalar, Index, Dimension> mesh_to_simple_scene(SurfaceMesh<Scalar, Index> mesh)
{
    return meshes_to_simple_scene<Dimension, Scalar, Index>({std::move(mesh)});
}

template <size_t Dimension, typename Scalar, typename Index>
SimpleScene<Scalar, Index, Dimension> meshes_to_simple_scene(
    std::vector<SurfaceMesh<Scalar, Index>> meshes)
{
    SimpleScene<Scalar, Index, Dimension> scene;
    scene.reserve_meshes(static_cast<Index>(meshes.size()));
    for (auto& mesh : meshes) {
        la_runtime_assert(
            mesh.get_dimension() == static_cast<Index>(Dimension),
            "Incompatible mesh dimension");
        auto mesh_index = scene.add_mesh(std::move(mesh));
        scene.add_instance({mesh_index});
    }
    return scene;
}

template <typename Scalar, typename Index, size_t Dimension>
SurfaceMesh<Scalar, Index> simple_scene_to_mesh(
    const SimpleScene<Scalar, Index, Dimension>& scene,
    const TransformOptions& transform_options,
    bool preserve_attributes)
{
    std::vector<SurfaceMesh<Scalar, Index>> meshes;
    meshes.reserve(scene.compute_num_instances());

    scene.foreach_instances([&](const auto& instance) {
        meshes.emplace_back(transformed_mesh<Scalar, Index>(
            scene.get_mesh(instance.mesh_index),
            instance.transform,
            transform_options));
    });
    return combine_meshes<Scalar, Index>(meshes, preserve_attributes);
}

#define LA_X_simple_scene_convert(_, Scalar, Index, Dimension)             \
    template LA_SCENE_API SimpleScene<Scalar, Index, Dimension> mesh_to_simple_scene(   \
        SurfaceMesh<Scalar, Index> mesh);                                  \
    template LA_SCENE_API SimpleScene<Scalar, Index, Dimension> meshes_to_simple_scene( \
        std::vector<SurfaceMesh<Scalar, Index>> meshes);                   \
    template LA_SCENE_API SurfaceMesh<Scalar, Index> simple_scene_to_mesh(              \
        const SimpleScene<Scalar, Index, Dimension>& scene,                \
        const TransformOptions& transform_options,                         \
        bool preserve_attributes);
LA_SIMPLE_SCENE_X(simple_scene_convert, 0)

} // namespace lagrange::scene

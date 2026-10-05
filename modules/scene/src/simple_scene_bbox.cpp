/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/scene/simple_scene_bbox.h>

#include <lagrange/mesh_bbox.h>
#include <lagrange/scene/SimpleSceneTypes.h>

namespace lagrange::scene {

template <typename Scalar, typename Index, size_t Dimension>
Eigen::AlignedBox<Scalar, static_cast<int>(Dimension)> simple_scene_bbox(
    const SimpleScene<Scalar, Index, Dimension>& scene)
{
    Eigen::AlignedBox<Scalar, static_cast<int>(Dimension)> bbox;
    for (Index mesh_index = 0; mesh_index < scene.get_num_meshes(); ++mesh_index) {
        const auto& mesh = scene.get_mesh(mesh_index);
        for (Index instance_index = 0; instance_index < scene.get_num_instances(mesh_index);
             ++instance_index) {
            bbox.extend(
                mesh_bbox<Dimension>(
                    mesh,
                    scene.get_instance(mesh_index, instance_index).transform));
        }
    }
    return bbox;
}

#define LA_X_simple_scene_bbox(_, Scalar, Index, Dimension)                      \
    template LA_SCENE_API Eigen::AlignedBox<Scalar, static_cast<int>(Dimension)> \
    simple_scene_bbox(const SimpleScene<Scalar, Index, Dimension>&);
LA_SIMPLE_SCENE_X(simple_scene_bbox, 0)

} // namespace lagrange::scene

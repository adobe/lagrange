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
#include <lagrange/scene/internal/bake_scaling.h>

#include <lagrange/Logger.h>
#include <lagrange/scene/SimpleSceneTypes.h>
#include <lagrange/utils/assert.h>

namespace lagrange::scene::internal {

namespace {

template <typename Scalar, typename Index, size_t Dimension = 3>
struct UserData
{
    using AffineTransform = Eigen::Transform<Scalar, static_cast<int>(Dimension), Eigen::Affine>;

    AffineTransform prev_transform;

    std::any prev_data;
};

} // namespace

template <typename Scalar, typename Index, size_t Dimension>
SimpleScene<Scalar, Index, Dimension> bake_scaling(
    SimpleScene<Scalar, Index, Dimension> scene,
    const std::vector<float>& per_instance_scaling)
{
    using Data = UserData<Scalar, Index, Dimension>;

    la_runtime_assert(
        per_instance_scaling.size() == scene.compute_num_instances(),
        "Per-instance scaling vector must have the same size as the total number of instances in "
        "the scene.");

    for (Index mesh_index = 0, global_index = 0; mesh_index < scene.get_num_meshes();
         ++mesh_index) {
        for (Index instance_index = 0; instance_index < scene.get_num_instances(mesh_index);
             ++instance_index, ++global_index) {
            auto& instance = scene.ref_instance(mesh_index, instance_index);
            instance.user_data = Data{instance.transform, std::move(instance.user_data)};
            instance.transform.scale(per_instance_scaling[global_index]);
            logger().debug(
                "Baking scaling factor {} into mesh {}, instance {}",
                per_instance_scaling[global_index],
                mesh_index,
                instance_index);
        }
    }

    return scene;
}

template <typename Scalar, typename Index, size_t Dimension>
SimpleScene<Scalar, Index, Dimension> unbake_scaling(SimpleScene<Scalar, Index, Dimension> scene)
{
    using Data = UserData<Scalar, Index, Dimension>;
    for (Index mesh_index = 0; mesh_index < scene.get_num_meshes(); ++mesh_index) {
        for (Index instance_index = 0; instance_index < scene.get_num_instances(mesh_index);
             ++instance_index) {
            auto& instance = scene.ref_instance(mesh_index, instance_index);
            Data* data = std::any_cast<Data>(&instance.user_data);
            la_runtime_assert(
                data,
                fmt::format(
                    "Cannot unbake scaling for instance {} of mesh {}. No previous transform was "
                    "found.",
                    instance_index,
                    mesh_index));
            instance.transform = data->prev_transform;
            instance.user_data = std::move(data->prev_data);
        }
    }

    return scene;
}

#define LA_X_bake_scaling(_, Scalar, Index, Dimension)                          \
    template LA_SCENE_API SimpleScene<Scalar, Index, Dimension> bake_scaling(   \
        SimpleScene<Scalar, Index, Dimension> scene,                            \
        const std::vector<float>& per_instance_scaling);                        \
    template LA_SCENE_API SimpleScene<Scalar, Index, Dimension> unbake_scaling( \
        SimpleScene<Scalar, Index, Dimension> scene);
LA_SIMPLE_SCENE_X(bake_scaling, 0)

} // namespace lagrange::scene::internal

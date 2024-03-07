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
#include <lagrange/scene/cast.h>

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/utils/assert.h>

namespace lagrange::scene {

template <
    typename ToScalar,
    typename ToIndex,
    typename FromScalar,
    typename FromIndex,
    size_t Dimension>
SimpleScene<ToScalar, ToIndex, Dimension> cast(
    const scene::SimpleScene<FromScalar, FromIndex, Dimension>& source_scene,
    const AttributeFilter& convertible_attributes,
    std::vector<std::string>* converted_attributes_names)
{
    using TargetSceneType = scene::SimpleScene<ToScalar, ToIndex, Dimension>;
    TargetSceneType target_scene;
    target_scene.reserve_meshes(source_scene.get_num_meshes());

    std::vector<std::string> tmp_attribute_names;
    std::vector<std::string>* tmp_attribute_names_ptr = nullptr;
    if (converted_attributes_names) {
        tmp_attribute_names_ptr = &tmp_attribute_names;
    }

    bool first = true;
    for (FromIndex mesh_index = 0; mesh_index < source_scene.get_num_meshes(); ++mesh_index) {
        auto converted_mesh = cast<ToScalar, ToIndex>(
            source_scene.get_mesh(mesh_index),
            convertible_attributes,
            tmp_attribute_names_ptr);
        target_scene.add_mesh(std::move(converted_mesh));
        target_scene.reserve_instances(mesh_index, source_scene.get_num_instances(mesh_index));
        if (converted_attributes_names && first) {
            *converted_attributes_names = tmp_attribute_names;
            first = false;
        }
        if (converted_attributes_names) {
            la_runtime_assert(*converted_attributes_names == tmp_attribute_names);
        }
    }
    source_scene.foreach_instances([&](auto& instance) {
        typename TargetSceneType::InstanceType converted_instance;
        converted_instance.mesh_index = instance.mesh_index;
        converted_instance.transform =
            typename TargetSceneType::AffineTransform(instance.transform);
        converted_instance.user_data = std::move(instance.user_data);
        target_scene.add_instance(converted_instance);
    });

    return target_scene;
}

// NOTE: This is a dirty workaround because nesting two LA_SURFACE_MESH_X macros doesn't quite work.
// I am not sure if there is a proper way to do this.
#define LA_SURFACE_MESH2_X(mode, data)                                     \
    LA_X_##mode(data, float, uint32_t) LA_X_##mode(data, double, uint32_t) \
        LA_X_##mode(data, float, uint64_t) LA_X_##mode(data, double, uint64_t)

// Iterate over mesh (scalar, index) x (scalar, index) types
#define fst(first, second) first
#define snd(first, second) second
#define LA_X_cast_mesh_to(FromScalarIndex, ToScalar, ToIndex)                         \
    template SimpleScene<ToScalar, ToIndex, 2u> cast(                                 \
        const SimpleScene<fst FromScalarIndex, snd FromScalarIndex, 2u>& source_mesh, \
        const AttributeFilter& convertible_attributes,                                \
        std::vector<std::string>* converted_attributes_names);                        \
    template SimpleScene<ToScalar, ToIndex, 3u> cast(                                 \
        const SimpleScene<fst FromScalarIndex, snd FromScalarIndex, 3u>& source_mesh, \
        const AttributeFilter& convertible_attributes,                                \
        std::vector<std::string>* converted_attributes_names);
#define LA_X_cast_mesh_from(_, FromScalar, FromIndex) \
    LA_SURFACE_MESH2_X(cast_mesh_to, (FromScalar, FromIndex))
LA_SURFACE_MESH_X(cast_mesh_from, 0)

} // namespace lagrange::scene

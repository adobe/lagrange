/*
 * Copyright 2022 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/scene/SimpleScene.h>

#include <lagrange/scene/SimpleSceneTypes.h>
#include <lagrange/utils/assert.h>

namespace lagrange::scene {

template <typename Scalar, typename Index, size_t Dimension>
Index SimpleScene<Scalar, Index, Dimension>::compute_num_instances() const
{
    Index total = 0;
    for (Index i = 0; i < get_num_meshes(); ++i) {
        total += get_num_instances(i);
    }
    return total;
}

template <typename Scalar, typename Index, size_t Dimension>
void SimpleScene<Scalar, Index, Dimension>::reserve_meshes(Index num_meshes)
{
    m_meshes.reserve(num_meshes);
    m_instances.reserve(num_meshes);
}

template <typename Scalar, typename Index, size_t Dimension>
Index SimpleScene<Scalar, Index, Dimension>::add_mesh(MeshType mesh)
{
    Index mesh_index = static_cast<Index>(m_meshes.size());
    m_meshes.emplace_back(std::move(mesh));
    m_instances.emplace_back();
    return mesh_index;
}

template <typename Scalar, typename Index, size_t Dimension>
void SimpleScene<Scalar, Index, Dimension>::reserve_instances(Index mesh_index, Index num_instances)
{
    m_instances[mesh_index].reserve(num_instances);
}

template <typename Scalar, typename Index, size_t Dimension>
Index SimpleScene<Scalar, Index, Dimension>::add_instance(InstanceType instance)
{
    la_runtime_assert(instance.mesh_index < static_cast<Index>(m_instances.size()));
    Index instance_index = static_cast<Index>(m_instances[instance.mesh_index].size());
    m_instances[instance.mesh_index].emplace_back(std::move(instance));
    return instance_index;
}

template <typename Scalar, typename Index, size_t Dimension>
void SimpleScene<Scalar, Index, Dimension>::foreach_instances_for_mesh(
    Index mesh_index,
    function_ref<void(const InstanceType&)> func) const
{
    for (const auto& instance : m_instances[mesh_index]) {
        func(instance);
    }
}

template <typename Scalar, typename Index, size_t Dimension>
void SimpleScene<Scalar, Index, Dimension>::foreach_instances(
    function_ref<void(const InstanceType&)> func) const
{
    for (Index mesh_index = 0; mesh_index < get_num_meshes(); ++mesh_index) {
        foreach_instances_for_mesh(mesh_index, func);
    }
}

#define LA_X_simple_scene(_, Scalar, Index, Dim) \
    template class LA_SCENE_API SimpleScene<Scalar, Index, Dim>;
LA_SIMPLE_SCENE_X(simple_scene, 0)

} // namespace lagrange::scene

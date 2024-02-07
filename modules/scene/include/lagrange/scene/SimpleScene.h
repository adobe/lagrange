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
#pragma once

#include <lagrange/SurfaceMesh.h>
#include <lagrange/utils/invalid.h>

#include <Eigen/Geometry>

#include <any>
#include <vector>

namespace lagrange::scene {

///
/// A single mesh instance in a scene.
///
/// @tparam     Scalar     Mesh scalar type.
/// @tparam     Index      Mesh index type.
/// @tparam     Dimension  Mesh dimension. Needed since we store the instance transform as a
///                        stack-allocated matrix.
///
template <typename Scalar, typename Index, size_t Dimension = 3>
struct MeshInstance
{
    /// Affine transformation matrix.
    using AffineTransform = Eigen::Transform<Scalar, static_cast<int>(Dimension), Eigen::Affine>;

    /// Index of the referenced mesh in the scene.
    Index mesh_index = invalid<Index>();

    /// Instance transformation.
    AffineTransform transform = AffineTransform::Identity();

    /// Opaque user data.
    std::any user_data = {};

    /// Access dimension from outside the class
    constexpr static size_t Dim = Dimension;
};

///
/// Simple scene container for instanced meshes.
///
/// @tparam     Scalar     Mesh scalar type.
/// @tparam     Index      Mesh index type.
/// @tparam     Dimension  Mesh dimension.
///
template <typename Scalar, typename Index, size_t Dimension = 3>
class SimpleScene
{
public:
    /// Mesh type.
    using MeshType = SurfaceMesh<Scalar, Index>;

    /// Instance type.
    using InstanceType = MeshInstance<Scalar, Index, Dimension>;

    /// Affine transform matrix.
    using AffineTransform = typename InstanceType::AffineTransform;

    /// Access dimension from outside the class
    constexpr static size_t Dim = Dimension;

public:
    ///
    /// Gets the number of meshes in the scene.
    ///
    /// @return     Number of meshes.
    ///
    Index get_num_meshes() const { return static_cast<Index>(m_meshes.size()); }

    ///
    /// Gets the number of instances for a given mesh.
    ///
    /// @param[in]  mesh_index  Mesh index.
    ///
    /// @return     Number of instances for a given mesh.
    ///
    Index get_num_instances(Index mesh_index) const
    {
        return static_cast<Index>(m_instances[mesh_index].size());
    }

    ///
    /// Calculates the total number instances for all meshes in the scene.
    ///
    /// @return     Total number of instances in the scene.
    ///
    Index compute_num_instances() const;

    ///
    /// Gets a const reference to a mesh in the scene.
    ///
    /// @param[in]  mesh_index  Mesh index.
    ///
    /// @return     Reference to the specified mesh.
    ///
    const MeshType& get_mesh(Index mesh_index) const { return m_meshes[mesh_index]; }

    ///
    /// Gets a modifiable reference to a mesh in a scene.
    ///
    /// @param[in]  mesh_index  Mesh index.
    ///
    /// @return     Reference to the specified mesh.
    ///
    MeshType& ref_mesh(Index mesh_index) { return m_meshes[mesh_index]; }

    ///
    /// Get a const reference to a mesh instance in the scene.
    ///
    /// @param[in]  mesh_index      Index of the parent mesh in the scene.
    /// @param[in]  instance_index  Local instance index respective to the parent mesh.
    ///
    /// @return     Reference to the specified mesh instance.
    ///
    const InstanceType& get_instance(Index mesh_index, Index instance_index) const
    {
        return m_instances[mesh_index][instance_index];
    }

    ///
    /// Pre-allocate a number of meshes in the scene.
    ///
    /// @param[in]  num_meshes  Number of meshes to reserve in the scene.
    ///
    void reserve_meshes(Index num_meshes);

    ///
    /// Adds a mesh to the scene, possibly with existing instances.
    ///
    /// @param[in]  mesh  Mesh to be added to the scene. The object will be moved into the scene.
    ///
    /// @return     Index of the newly added mesh in the scene.
    ///
    Index add_mesh(MeshType mesh);

    ///
    /// Pre-allocate a number of instances for a given mesh.
    ///
    /// @param[in]  mesh_index     Mesh index.
    /// @param[in]  num_instances  Number of instances to reserve for this mesh.
    ///
    void reserve_instances(Index mesh_index, Index num_instances);

    ///
    /// Adds a new instance of an existing mesh.
    ///
    /// @param[in]  instance  Mesh instance to add to the scene.
    ///
    /// @return     Index of the newly added instance, respective to the specified mesh.
    ///
    Index add_instance(InstanceType instance);

    ///
    /// Iterates over all instances of a specific mesh.
    ///
    /// @param[in]  mesh_index  Mesh index on which to iterate over.
    /// @param[in]  func        Callback function to call for each mesh instance.
    ///
    void foreach_instances_for_mesh(Index mesh_index, function_ref<void(const InstanceType&)> func)
        const;

    ///
    /// Iterates over all instances of the scene.
    ///
    /// @param[in]  func  Callback function to call for each mesh instance.
    ///
    void foreach_instances(function_ref<void(const InstanceType&)> func) const;

protected:
    /// List of meshes in the scene.
    std::vector<MeshType> m_meshes;

    /// List of mesh instances in the scene. Stored as a list of instance per parent mesh.
    std::vector<std::vector<InstanceType>> m_instances;
};

using SimpleScene32f2 = SimpleScene<float, uint32_t, 2u>;
using SimpleScene32d2 = SimpleScene<double, uint32_t, 2u>;
using SimpleScene64f2 = SimpleScene<float, uint64_t, 2u>;
using SimpleScene64d2 = SimpleScene<double, uint64_t, 2u>;
using SimpleScene32f3 = SimpleScene<float, uint32_t, 3u>;
using SimpleScene32d3 = SimpleScene<double, uint32_t, 3u>;
using SimpleScene64f3 = SimpleScene<float, uint64_t, 3u>;
using SimpleScene64d3 = SimpleScene<double, uint64_t, 3u>;

} // namespace lagrange::scene

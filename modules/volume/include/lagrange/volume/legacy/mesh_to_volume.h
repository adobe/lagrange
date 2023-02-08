/*
 * Copyright 2021 Adobe. All rights reserved.
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

#include <lagrange/Logger.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/legacy/inline.h>

#include <openvdb/openvdb.h>
#include <openvdb/tools/MeshToVolume.h>
#include <Eigen/Dense>

namespace lagrange {
namespace volume {
LAGRANGE_LEGACY_INLINE
namespace legacy {

///
/// Adapter class to interface a Lagrange mesh with OpenVDB functions.
///
template <typename MeshType>
class MeshAdapter
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

public:
    ///
    /// Constructs a new instance.
    ///
    /// @param[in]  mesh       Input mesh.
    /// @param[in]  transform  World to index transform.
    ///
    MeshAdapter(const MeshType& mesh, const openvdb::math::Transform& transform)
        : m_mesh(mesh)
        , m_transform(transform)
    {}

    /// Number of mesh facets.
    size_t polygonCount() const { return static_cast<size_t>(m_mesh.get_num_facets()); }

    /// Number of mesh vertices.
    size_t pointCount() const { return static_cast<size_t>(m_mesh.get_num_vertices()); }

    /// Number of vertices for a given facet.
    size_t vertexCount(size_t /*f*/) const
    {
        return static_cast<size_t>(m_mesh.get_vertex_per_facet());
    }

    ///
    /// Return a vertex position in the grid index space.
    ///
    /// @param[in]  f     Queried facet index.
    /// @param[in]  lv    Queried local vertex index.
    /// @param[out] pos   Vertex position in grid index space.
    ///
    void getIndexSpacePoint(size_t f, size_t lv, openvdb::Vec3d& pos) const
    {
        Eigen::RowVector3d p =
            m_mesh.get_vertices().row(m_mesh.get_facets()(f, lv)).template cast<double>();
        pos = openvdb::Vec3d(p.x(), p.y(), p.z());
        pos = m_transform.worldToIndex(pos);
    }

protected:
    const MeshType& m_mesh;
    const openvdb::math::Transform& m_transform;
};

///
/// Converts a triangle mesh to a OpenVDB sparse voxel grid.
///
/// @param[in]  mesh        Input mesh.
/// @param[in]  voxel_size  Grid voxel size. If the target voxel size is too small, an exception
///                         will will be raised.
///
/// @tparam     MeshType    Mesh type.
/// @tparam     GridType    OpenVDB grid type.
///
/// @return     OpenVDB grid.
///
template <typename MeshType, typename GridType = openvdb::FloatGrid>
auto mesh_to_volume(const MeshType& mesh, double voxel_size) -> typename GridType::Ptr
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    openvdb::initialize();

    const openvdb::Vec3d offset(voxel_size / 2.0, voxel_size / 2.0, voxel_size / 2.0);
    auto transform = openvdb::math::Transform::createLinearTransform(voxel_size);
    transform->postTranslate(offset);

    MeshAdapter<MeshType> adapter(mesh, *transform);
    try {
        return openvdb::tools::meshToVolume<GridType, MeshAdapter<MeshType>>(adapter, *transform);
    } catch (openvdb::ArithmeticError&) {
        logger().error("Voxel size too small: {}", voxel_size);
        throw;
    }
}

} // namespace legacy
} // namespace volume
} // namespace lagrange

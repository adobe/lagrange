/*
 * Copyright 2023 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/volume/mesh_to_volume.h>

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>
#include <lagrange/volume/GridTypes.h>
#include <lagrange/winding/FastWindingNumber.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <openvdb/tools/ValueTransformer.h>
#include <openvdb/tools/MeshToVolume.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::volume {

namespace {

///
/// Adapter class to interface a Lagrange mesh with OpenVDB functions.
///
template <typename Scalar, typename Index>
class SurfaceMeshAdapter
{
public:
    ///
    /// Constructs a new instance.
    ///
    /// @param[in]  mesh       Input mesh.
    /// @param[in]  transform  World to index transform.
    ///
    SurfaceMeshAdapter(
        const SurfaceMesh<Scalar, Index>& mesh,
        const openvdb::math::Transform& transform)
        : m_mesh(mesh)
        , m_transform(transform)
    {}

    /// Number of mesh facets.
    size_t polygonCount() const { return static_cast<size_t>(m_mesh.get_num_facets()); }

    /// Number of mesh vertices.
    size_t pointCount() const { return static_cast<size_t>(m_mesh.get_num_vertices()); }

    /// Number of vertices for a given facet.
    size_t vertexCount(size_t f) const
    {
        return static_cast<size_t>(m_mesh.get_facet_size(static_cast<Index>(f)));
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
        auto p = m_mesh.get_position(
            m_mesh.get_facet_vertex(static_cast<Index>(f), static_cast<Index>(lv)));
        pos = openvdb::Vec3d(p[0], p[1], p[2]);
        pos = m_transform.worldToIndex(pos);
    }

protected:
    const SurfaceMesh<Scalar, Index>& m_mesh;
    const openvdb::math::Transform& m_transform;
};

} // namespace

template <typename GridScalar, typename Scalar, typename Index>
auto mesh_to_volume(const SurfaceMesh<Scalar, Index>& mesh_, const MeshToVolumeOptions& options) ->
    typename Grid<GridScalar>::Ptr
{
    static_assert(
        std::is_same_v<
            Grid<GridScalar>,
            openvdb::Grid<typename openvdb::tree::Tree4<GridScalar, 5, 4, 3>::Type>>,
        "Mismatch between VDB grid types!");

    auto mesh = SurfaceMesh<Scalar, Index>::stripped_copy(mesh_);
    la_runtime_assert(mesh.get_dimension() == 3, "Input mesh must be 3D");
    if (mesh.is_hybrid()) {
        for (Index f = 0; f < mesh.get_num_facets(); ++f) {
            if (auto nv = mesh.get_facet_size(f); nv < 3 || nv > 4) {
                throw Error(
                    fmt::format("Facet size should be 3 or 4, but f{} has #{} vertices", f, nv));
            }
        }
    }

    // Winding number requires triangle meshes. To ensure consistent discretization, we triangulate
    // before letting OpenVDB compute the unsigned distance field.
    if (options.signing_method == MeshToVolumeOptions::Sign::WindingNumber) {
        if (!mesh.is_triangle_mesh()) {
            triangulate_polygonal_facets(mesh);
        }
    }

    openvdb::initialize();

    auto voxel_size = options.voxel_size;
    if (voxel_size < 0) {
        // Compute bbox
        Eigen::AlignedBox<Scalar, 3> bbox;
        for (auto p : vertex_view(mesh).rowwise()) {
            bbox.extend(p.transpose());
        }

        const Scalar diag = bbox.diagonal().norm();
        voxel_size = std::abs(voxel_size);
        lagrange::logger().debug(
            "Using a relative voxel size of {:.3f} x {:.3f} = {:.3f}",
            voxel_size,
            diag,
            voxel_size * diag);
        voxel_size *= diag;
    }

    const openvdb::Vec3d offset(voxel_size / 2.0, voxel_size / 2.0, voxel_size / 2.0);
    auto transform = openvdb::math::Transform::createLinearTransform(voxel_size);
    transform->postTranslate(offset);

    using MeshAdapterType = SurfaceMeshAdapter<Scalar, Index>;

    MeshAdapterType adapter(mesh, *transform);
    typename Grid<GridScalar>::Ptr grid;
    try {
        if (options.signing_method == MeshToVolumeOptions::Sign::WindingNumber) {
            // Initialize fast winding number engine.
            logger().debug("Initializing fast winding number engine");
            la_debug_assert(mesh.is_triangle_mesh());
            winding::FastWindingNumber engine(mesh);

            // Use winding number to sign the distance field
            logger().debug("Computing distance field with winding number signing");
            const float exterior_bandwidth = 3.0f;
            const float interior_bandwidth = 3.0f;
            openvdb::util::NullInterrupter null_interrupter;
            grid = openvdb::tools::meshToVolume<Grid<GridScalar>, MeshAdapterType>(
                null_interrupter,
                adapter,
                *transform,
                exterior_bandwidth,
                interior_bandwidth,
                0 /* flags */,
                nullptr /* polygonIndexGrid */,
                [transform, &engine](openvdb::Coord ijk) {
                    openvdb::Vec3d pos = transform->indexToWorld(ijk);
                    const std::array<float, 3> p = {
                        static_cast<float>(pos[0]),
                        static_cast<float>(pos[1]),
                        static_cast<float>(pos[2])};
                    return engine.is_inside(p);
                },
                openvdb::tools::EVAL_EVERY_VOXEL);
        } else if (options.signing_method == MeshToVolumeOptions::Sign::Unsigned) {
            // Compute unsigned distance field
            const float exterior_bandwidth = 3.0f;
            const float interior_bandwidth = 3.0f;
            grid = openvdb::tools::meshToVolume<Grid<GridScalar>, MeshAdapterType>(
                adapter,
                *transform,
                exterior_bandwidth,
                interior_bandwidth,
                openvdb::tools::UNSIGNED_DISTANCE_FIELD);
        } else {
            grid = openvdb::tools::meshToVolume<Grid<GridScalar>, MeshAdapterType>(
                adapter,
                *transform);
        }
    } catch (const openvdb::ArithmeticError&) {
        logger().error("Voxel size too small: {}", voxel_size);
        throw;
    }

    logger().debug("Computed grid has {} active voxels", grid->activeVoxelCount());

    return grid;
}

#define LA_X_mesh_to_volume(GridScalar, Scalar, Index)                  \
    template typename Grid<GridScalar>::Ptr mesh_to_volume<GridScalar>( \
        const SurfaceMesh<Scalar, Index>& mesh,                         \
        const MeshToVolumeOptions& options);
#define LA_X_mesh_to_volume_aux(_, GridScalar) LA_SURFACE_MESH_X(mesh_to_volume, GridScalar)
LA_VOLUME_GRID_X(mesh_to_volume_aux, 0)

} // namespace lagrange::volume

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

#include <openvdb/tools/ValueTransformer.h>

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
auto mesh_to_volume(const SurfaceMesh<Scalar, Index>& mesh, const MeshToVolumeOptions& options) ->
    typename Grid<GridScalar>::Ptr
{
    static_assert(
        std::is_same_v<
            Grid<GridScalar>,
            openvdb::Grid<typename openvdb::tree::Tree4<GridScalar, 5, 4, 3>::Type>>,
        "Mismatch between VDB grid types!");

    la_runtime_assert(mesh.get_dimension() == 3, "Input mesh must be 3D");
    if (mesh.is_hybrid()) {
        for (Index f = 0; f < mesh.get_num_facets(); ++f) {
            if (auto nv = mesh.get_facet_size(f); nv < 3 || nv > 4) {
                throw Error(
                    fmt::format("Facet size should be 3 or 4, but f{} has #{} vertices", f, nv));
            }
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
            // Two stage grid signing approach
            {
                // Compute unsigned distance field
                logger().debug("Computing unsigned distance field grid");
                const float exterior_bandwidth = 3.0f;
                const float interior_bandwidth = 3.0f;
                grid = openvdb::tools::meshToVolume<Grid<GridScalar>, MeshAdapterType>(
                    adapter,
                    *transform,
                    exterior_bandwidth,
                    interior_bandwidth,
                    openvdb::tools::UNSIGNED_DISTANCE_FIELD);
            }
            {
                // Initialize fast winding number engine.
                //
                // TODO: Drop mesh attributes from copy to avoid remapping attributes that may be
                // present in the input mesh.
                logger().debug("Initializing fast winding number engine");
                auto triangle_mesh = mesh;
                if (!triangle_mesh.is_triangle_mesh()) {
                    // Triangulate quad facets first if needed
                    triangulate_polygonal_facets(triangle_mesh);
                }
                // Apply world->index transform (query points for winding number have coords in
                // index space)
                for (auto p : vertex_ref(triangle_mesh).rowwise()) {
                    openvdb::Vec3d pos(p[0], p[1], p[2]);
                    pos = transform->worldToIndex(pos);
                    p << pos[0], pos[1], pos[2];
                }
                winding::FastWindingNumber engine(triangle_mesh);

                // Iterate over all grid values (both voxel and tile, active and inactive)
                logger().debug("Applying fast winding number sign to the grid");
                auto sign = [&](auto&& iter) {
                    const auto pos = iter.getBoundingBox().getCenter();
                    const std::array<float, 3> p = {
                        static_cast<float>(pos[0]),
                        static_cast<float>(pos[1]),
                        static_cast<float>(pos[2])};
                    if (engine.is_inside(p)) {
                        iter.setValue(-*iter);
                    }
                };
                openvdb::tools::foreach (grid->beginValueAll(), sign, true /* threaded */);
                logger().debug("Done computing grid");
            }
        } else {
            grid = openvdb::tools::meshToVolume<Grid<GridScalar>, MeshAdapterType>(
                adapter,
                *transform);
        }
    } catch (const openvdb::ArithmeticError&) {
        logger().error("Voxel size too small: {}", voxel_size);
        throw;
    }

    return grid;
}

#define LA_X_mesh_to_volume(GridScalar, Scalar, Index)                  \
    template typename Grid<GridScalar>::Ptr mesh_to_volume<GridScalar>( \
        const SurfaceMesh<Scalar, Index>& mesh,                         \
        const MeshToVolumeOptions& options);
#define LA_X_mesh_to_volume_aux(_, GridScalar) LA_SURFACE_MESH_X(mesh_to_volume, GridScalar)
LA_VOLUME_GRID_X(mesh_to_volume_aux, 0)

} // namespace lagrange::volume

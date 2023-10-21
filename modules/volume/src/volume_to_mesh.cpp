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
#include <lagrange/volume/volume_to_mesh.h>

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/volume/GridTypes.h>

namespace lagrange::volume {

template <typename MeshType, typename GridScalar>
auto volume_to_mesh(const Grid<GridScalar>& grid, const VolumeToMeshOptions& options)
    -> SurfaceMesh<typename MeshType::Scalar, typename MeshType::Index>
{
    openvdb::initialize();

    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;

    auto adaptivity = options.adaptivity;

    if (adaptivity < 0 || adaptivity > 1) {
        logger().warn("Adaptivity needs to be between 0 and 1.");
        adaptivity = std::clamp(adaptivity, 0.0, 1.0);
    }

    std::vector<openvdb::Vec3s> points;
    std::vector<openvdb::Vec3I> triangles;
    std::vector<openvdb::Vec4I> quads;

    openvdb::tools::volumeToMesh(
        grid,
        points,
        triangles,
        quads,
        options.isovalue,
        adaptivity,
        options.relax_disoriented_triangles);

    // Level set grids need their facet flipped
    const bool need_flip = (grid.getGridClass() == openvdb::GRID_LEVEL_SET);

    SurfaceMesh<Scalar, Index> mesh;

    mesh.add_vertices(static_cast<Index>(points.size()), [&](Index v, auto p) {
        std::copy_n(points[v].asV(), 3, p.data());
    });

    mesh.add_triangles(static_cast<Index>(triangles.size()), [&](Index f, auto t) {
        std::copy_n(triangles[f].asV(), 3, t.data());
        if (need_flip) {
            std::reverse(t.begin(), t.end());
        }
    });

    mesh.add_quads(static_cast<Index>(quads.size()), [&](Index f, auto t) {
        std::copy_n(quads[f].asV(), 4, t.data());
        if (need_flip) {
            std::reverse(t.begin(), t.end());
        }
    });

    return mesh;
}

#define LA_X_volume_to_mesh(GridScalar, Scalar, Index)                              \
    template SurfaceMesh<Scalar, Index> volume_to_mesh<SurfaceMesh<Scalar, Index>>( \
        const Grid<GridScalar>& grid,                                               \
        const VolumeToMeshOptions& options);
#define LA_X_volume_to_mesh_aux(_, GridScalar) LA_SURFACE_MESH_X(volume_to_mesh, GridScalar)
LA_VOLUME_GRID_X(volume_to_mesh_aux, 0)

} // namespace lagrange::volume

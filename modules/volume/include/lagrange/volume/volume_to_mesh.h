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

#include <lagrange/create_mesh.h>

#include <openvdb/tools/VolumeToMesh.h>
#include <openvdb/openvdb.h>

namespace lagrange {
namespace volume {

///
/// Mesh the isosurface of a OpenVDB sparse voxel grid.
///
/// @param[in]  grid                         Input grid.
/// @param[in]  isovalue                     Determines which isosurface to mesh.
/// @param[in]  adaptivity                   Surface adaptivity threshold [0 to 1]. 0 keeps the
///                                          original quad mesh, while 1 simplifies the most.
/// @param[in]  relax_disoriented_triangles  Toggle relaxing disoriented triangles during adaptive
///                                          meshing.
///
/// @tparam     MeshType                     Mesh type.
/// @tparam     GridType                     OpenVDB grid type.
///
/// @return     Meshed isosurface.
///
template <typename MeshType, typename GridType>
std::unique_ptr<MeshType> volume_to_mesh(
    const GridType &grid,
    double isovalue = 0.0,
    double adaptivity = 0.0,
    bool relax_disoriented_triangles = true)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    openvdb::initialize();

    using Scalar = ScalarOf<MeshType>;
    using Index = IndexOf<MeshType>;
    using VertexArray = VertexArrayOf<MeshType>;
    using FacetArray = FacetArrayOf<MeshType>;
    using RowVector3s = Eigen::Matrix<float, 1, 3>;
    using RowVector3I = Eigen::Matrix<openvdb::Index32, 1, 3>;
    using RowVector3I = Eigen::Matrix<openvdb::Index32, 1, 3>;
    using RowVector4I = Eigen::Matrix<openvdb::Index32, 1, 4>;
    using RowVector4i = Eigen::Matrix<Index, 1, 4>;

    if (adaptivity < 0 || adaptivity > 1) {
        logger().warn("Adaptivity needs to be between 0 and 1.");
        adaptivity = std::max(0.0, std::min(1.0, adaptivity));
    }

    std::vector<openvdb::Vec3s> points;
    std::vector<openvdb::Vec3I> triangles;
    std::vector<openvdb::Vec4I> quads;

    openvdb::tools::volumeToMesh(
        grid,
        points,
        triangles,
        quads,
        isovalue,
        adaptivity,
        relax_disoriented_triangles);

    VertexArray vertices(points.size(), 3);
    FacetArray facets(triangles.size() + 2 * quads.size(), 3);

    // Level set grids need their facet flipped
    const bool need_flip = (grid.getGridClass() == openvdb::GRID_LEVEL_SET);

    for (size_t v = 0; v < points.size(); ++v) {
        const RowVector3s p(points[v].x(), points[v].y(), points[v].z());
        vertices.row(v) << p.template cast<Scalar>();
    }

    for (size_t f = 0; f < triangles.size(); ++f) {
        const RowVector3I triangle(triangles[f].x(), triangles[f].y(), triangles[f].z());
        facets.row(f) << triangle.template cast<Index>();
        if (need_flip) {
            facets.row(f) = facets.row(f).reverse().eval();
        }
    }

    for (size_t f = 0, o = triangles.size(); f < quads.size(); ++f) {
        const RowVector4I quad_(quads[f].x(), quads[f].y(), quads[f].z(), quads[f].w());
        const RowVector4i quad = quad_.template cast<Index>();
        facets.row(o + 2 * f) << quad(0), quad(1), quad(3);
        facets.row(o + 2 * f + 1) << quad(3), quad(1), quad(2);
        if (need_flip) {
            facets.row(o + 2 * f) = facets.row(o + 2 * f).reverse().eval();
            facets.row(o + 2 * f + 1) = facets.row(o + 2 * f + 1).reverse().eval();
        }
    }

    return lagrange::create_mesh<VertexArray, FacetArray>(std::move(vertices), std::move(facets));
}

} // namespace volume
} // namespace lagrange

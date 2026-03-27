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

#include <lagrange/mesh_bbox.h>

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

namespace lagrange {

template <size_t Dimension, typename Scalar, typename Index>
Eigen::AlignedBox<Scalar, static_cast<int>(Dimension)> mesh_bbox(
    const SurfaceMesh<Scalar, Index>& mesh)
{
    static_assert(Dimension == 2 || Dimension == 3, "Only 2D and 3D meshes are supported.");
    la_runtime_assert(
        mesh.get_dimension() == Dimension,
        "Mesh dimension does not match the requested bounding box dimension.");
    const auto vertices = vertex_view(mesh);
    Eigen::AlignedBox<Scalar, static_cast<int>(Dimension)> bbox;
    if (vertices.rows() > 0) {
        bbox.min() = vertices.colwise().minCoeff().transpose();
        bbox.max() = vertices.colwise().maxCoeff().transpose();
    }
    return bbox;
}

#define LA_X_mesh_bbox(_, Scalar, Index)                                            \
    template LA_CORE_API Eigen::AlignedBox<Scalar, 2> mesh_bbox<2u, Scalar, Index>( \
        const SurfaceMesh<Scalar, Index>&);                                         \
    template LA_CORE_API Eigen::AlignedBox<Scalar, 3> mesh_bbox<3u, Scalar, Index>( \
        const SurfaceMesh<Scalar, Index>&);
LA_SURFACE_MESH_X(mesh_bbox, 0)

} // namespace lagrange

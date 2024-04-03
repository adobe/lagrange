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

#include <Eigen/Geometry>

#include <lagrange/SurfaceMesh.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/normalize_meshes.h>
#include <lagrange/views.h>

namespace lagrange {

template <typename Scalar, typename Index>
void normalize_mesh(SurfaceMesh<Scalar, Index>& mesh)
{
    auto vertices = vertex_ref(mesh);
    if (vertices.rows() == 0) return;

    auto bbox_min = vertices.colwise().minCoeff().eval();
    auto bbox_max = vertices.colwise().maxCoeff().eval();
    auto bbox_center = ((bbox_min + bbox_max) / 2).eval();
    Scalar s = (bbox_max - bbox_min).norm() / 2;
    if (s == 0) return;

    vertices.rowwise() -= bbox_center;
    vertices = vertices / s;
}

template <typename Scalar, typename Index>
void normalize_meshes(span<SurfaceMesh<Scalar, Index>*> meshes)
{
    Eigen::AlignedBox<Scalar, 3> bbox;
    for (auto mesh_ptr : meshes) {
        la_runtime_assert(mesh_ptr->get_dimension() == 3);
        auto vertices = vertex_view(*mesh_ptr);
        bbox.extend(vertices.colwise().minCoeff().transpose());
        bbox.extend(vertices.colwise().maxCoeff().transpose());
    }

    if (bbox.isEmpty()) return;
    Scalar s = (bbox.max() - bbox.min()).norm() / 2;
    if (s == 0) return;

    for (auto mesh_ptr : meshes) {
        auto vertices = vertex_ref(*mesh_ptr);
        vertices.rowwise() -= bbox.center().transpose();
        vertices = vertices / s;
    }
}

#define LA_X_normalize_meshes(_, Scalar, Index)                                     \
    template LA_CORE_API void normalize_mesh<Scalar, Index>(SurfaceMesh<Scalar, Index> & mesh); \
    template LA_CORE_API void normalize_meshes<Scalar, Index>(span<SurfaceMesh<Scalar, Index>*> meshes);
LA_SURFACE_MESH_X(normalize_meshes, 0)
} // namespace lagrange

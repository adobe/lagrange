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
#pragma once

#include <lagrange/SurfaceMesh.h>
#include <lagrange/views.h>

#include <Eigen/Core>

namespace lagrange {
///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various attribute processing utilities
///
/// @{

///
/// Create a SurfaceMesh from a igl-style pair of matrices (V, F).
///
/// @note       The target Scalar x Index type must be explicitly specified. In the future, we may
///             allow automatic deduction based on the input matrix types.
///
/// @param[in]  V         #V x d matrix of vertex positions.
/// @param[in]  F         #F x k matrix of facet indices. E.g. k=3 for triangle meshes, k=4 for quad
///                       meshes.
///
/// @tparam     Scalar    Target mesh scalar type (required). Either float or double.
/// @tparam     Index     Target mesh index type (required). Either uint32_t or uint64_t.
/// @tparam     DerivedV  Input vertex matrix type.
/// @tparam     DerivedF  Input facet matrix type.
///
/// @return     New mesh object.
///
template <typename Scalar, typename Index, typename DerivedV, typename DerivedF>
SurfaceMesh<Scalar, Index> eigen_to_surface_mesh(
    const Eigen::MatrixBase<DerivedV>& V,
    const Eigen::MatrixBase<DerivedF>& F)
{
    SurfaceMesh<Scalar, Index> mesh(static_cast<Index>(V.cols()));
    mesh.add_vertices(static_cast<Index>(V.rows()));
    mesh.add_polygons(static_cast<Index>(F.rows()), static_cast<Index>(F.cols()));
    vertex_ref(mesh) = V.template cast<Scalar>();
    facet_ref(mesh) = F.template cast<Index>();
    return mesh;
}

/// @}
} // namespace lagrange


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
#include "compute_weighted_corner_normal.h"

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

#include <Eigen/Dense>

namespace lagrange::internal {

template <typename Scalar, typename Index>
Eigen::Matrix<Scalar, 3, 1> compute_weighted_corner_normal(
    SurfaceMesh<Scalar, Index>& mesh,
    Index ci,
    NormalWeightingType weighting)
{
    la_debug_assert(mesh.get_dimension() == 3, "Only 3D meshes are supported.");

    using VectorType = Eigen::Matrix<Scalar, 3, 1>;
    Index fi = mesh.get_corner_facet(ci);
    Index fc_begin = mesh.get_facet_corner_begin(fi);
    Index fc_end = mesh.get_facet_corner_end(fi);
    la_debug_assert(ci >= fc_begin && ci < fc_end);
    Index c_next = (ci == (fc_end - 1)) ? fc_begin : ci + 1;
    Index c_prev = (ci == fc_begin) ? fc_end - 1 : ci - 1;

    Index v_curr = mesh.get_corner_vertex(ci);
    Index v_next = mesh.get_corner_vertex(c_next);
    Index v_prev = mesh.get_corner_vertex(c_prev);

    auto vertices = vertex_view(mesh);
    VectorType p_curr = vertices.row(v_curr).transpose();
    VectorType p_next = vertices.row(v_next).transpose();
    VectorType p_prev = vertices.row(v_prev).transpose();

    VectorType n = (p_next - p_curr).cross(p_prev - p_curr);

    switch (weighting) {
    case NormalWeightingType::Uniform: return n.stableNormalized();
    case NormalWeightingType::CornerTriangleArea: return n;
    case NormalWeightingType::Angle: {
        Scalar l = n.norm();
        n.stableNormalize();

        Scalar d = (p_next - p_curr).dot(p_prev - p_curr);
        Scalar theta = std::atan2(l, d);
        return n * theta;
    }
    default: throw Error("Unsupported weighting type detected.");
    }
}

#define LA_X_compute_weighted_corner_normal(_, Scalar, Index)                           \
    template Eigen::Matrix<Scalar, 3, 1> compute_weighted_corner_normal<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                                                    \
        Index,                                                                          \
        NormalWeightingType);
LA_SURFACE_MESH_X(compute_weighted_corner_normal, 0)

} // namespace lagrange::internal

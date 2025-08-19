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
    NormalWeightingType weighting,
    Scalar tol)
{
    la_debug_assert(mesh.get_dimension() == 3, "Only 3D meshes are supported.");
    const Scalar sq_tol = tol * tol;

    using VectorType = Eigen::Matrix<Scalar, 3, 1>;
    VectorType p_curr, p_next, p_prev;

    auto vertices = vertex_view(mesh);

    Index v_curr = mesh.get_corner_vertex(ci);
    p_curr = vertices.row(v_curr).transpose();

    Index fi = mesh.get_corner_facet(ci);
    Index fc_begin = mesh.get_facet_corner_begin(fi);
    Index fc_end = mesh.get_facet_corner_end(fi);
    la_debug_assert(ci >= fc_begin && ci < fc_end);
    bool is_triangle = (fc_end - fc_begin) == 3;

    Index c_next = (ci == (fc_end - 1)) ? fc_begin : ci + 1;
    Index v_next = mesh.get_corner_vertex(c_next);
    p_next = vertices.row(v_next).transpose();
    if (!is_triangle) {
        while (c_next != ci && (p_next - p_curr).squaredNorm() <= sq_tol) {
            // If the edge (curr, next) is degenerate, check the next-next corner.
            c_next = (c_next == (fc_end - 1)) ? fc_begin : c_next + 1;
            v_next = mesh.get_corner_vertex(c_next);
            p_next = vertices.row(v_next).transpose();
        }
    }

    Index c_prev = (ci == fc_begin) ? fc_end - 1 : ci - 1;
    Index v_prev = mesh.get_corner_vertex(c_prev);
    p_prev = vertices.row(v_prev).transpose();
    if (!is_triangle) {
        while (c_prev != ci && (p_prev - p_curr).squaredNorm() <= sq_tol) {
            // If the edge (curr, prev) is degenerate, check the previous-previous corner.
            c_prev = (c_prev == fc_begin) ? fc_end - 1 : c_prev - 1;
            v_prev = mesh.get_corner_vertex(c_prev);
            p_prev = vertices.row(v_prev).transpose();
        }
    }

    if (c_next == ci || c_prev == ci) {
        // The entire facet is degenerate.
        // As a fallback, reset to the immediate next and previous corners to attempt
        // to compute a valid normal.
        c_next = (ci == (fc_end - 1)) ? fc_begin : ci + 1;
        v_next = mesh.get_corner_vertex(c_next);
        p_next = vertices.row(v_next).transpose();

        c_prev = (ci == fc_begin) ? fc_end - 1 : ci - 1;
        v_prev = mesh.get_corner_vertex(c_prev);
        p_prev = vertices.row(v_prev).transpose();
    }

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
        NormalWeightingType,                                                            \
        Scalar);
LA_SURFACE_MESH_X(compute_weighted_corner_normal, 0)

} // namespace lagrange::internal

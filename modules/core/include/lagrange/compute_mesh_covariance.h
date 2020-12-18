/*
 * Copyright 2020 Adobe. All rights reserved.
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

#include <Eigen/Core>

#include <lagrange/Mesh.h>
#include <lagrange/utils/range.h>

namespace lagrange {

// Compute the covariance matrix
// mesh_ref, reference to the mesh
// center, the point around which the covariance is computed.
// active_facets, the facets that are included in the covariance computation.
//                Empty would imply all facets.
//
// Adapted from https://github.com/mkazhdan/ShapeSPH/blob/master/Util/TriangleMesh.h#L101
template <typename MeshType>
Eigen::Matrix<typename MeshType::Scalar, 3, 3> compute_mesh_covariance( //
    const MeshType &mesh_ref,
    const typename MeshType::VertexType &center,
    const typename MeshType::IndexList &active_facets = typename MeshType::IndexList())
{ //

    using Scalar = typename MeshType::Scalar;
    using Vector3 = Eigen::Matrix<Scalar, 1, 3>;
    using Matrix3 = Eigen::Matrix<Scalar, 3, 3>;

    const auto &vertices = mesh_ref.get_vertices();
    const auto &facets = mesh_ref.get_facets();

    LA_ASSERT(vertices.cols() == 3, "Currently, only 3 dimensions are supported");
    LA_ASSERT(facets.cols() == 3, "Currently, only triangles are supported");

    Matrix3 factors;
    factors.row(0) << Scalar(1. / 2), Scalar(1. / 3), Scalar(1. / 6);
    factors.row(1) << Scalar(1. / 3), Scalar(1. / 4), Scalar(1. / 8);
    factors.row(2) << Scalar(1. / 6), Scalar(1. / 8), Scalar(1. / 12);

    auto triangle_covariance = [&factors](
                                   const Vector3 &v1,
                                   const Vector3 &v2,
                                   const Vector3 &v3,
                                   const Vector3 &c) -> Matrix3 {
        const Scalar a = (v2 - v1).cross(v3 - v1).norm();
        Matrix3 p;
        p.row(0) = v1 - c;
        p.row(1) = v3 - v1;
        p.row(2) = v2 - v3;

        const Matrix3 covariance = a * p.transpose() * factors * p;

        return covariance;
    };

    Matrix3 covariance = Matrix3::Zero();
    for (auto facet_id : lagrange::range_facets(mesh_ref, active_facets)) {
        const Vector3 v0 = vertices.row(facets(facet_id, 0));
        const Vector3 v1 = vertices.row(facets(facet_id, 1));
        const Vector3 v2 = vertices.row(facets(facet_id, 2));
        covariance += triangle_covariance(v0, v1, v2, center);
    } // over triangles

    // Return
    return covariance;
}


} // namespace lagrange

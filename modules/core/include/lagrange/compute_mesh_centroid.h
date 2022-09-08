/*
 * Copyright 2019 Adobe. All rights reserved.
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

template <typename Scalar>
struct ComputeMeshCentroidOutput
{
    // We get this for free when computing the centroid.
    Scalar area;
    // The centroid itself. Will be row vector to be consistent with lagrange.
    Eigen::Matrix<Scalar, 1, Eigen::Dynamic> centroid;
};

// Compute the centroid of certain facets in a mesh
// mesh_ref, reference to the mesh
// active_facets, the facets that are included in the centroid computation.
//                Empty would imply all facets.
template <typename MeshType>
ComputeMeshCentroidOutput<typename MeshType::Scalar> compute_mesh_centroid( //
    const MeshType& mesh_ref,
    const typename MeshType::IndexList& active_facets = typename MeshType::IndexList())
{ //

    using Scalar = typename MeshType::Scalar;
    using Vertex3 = Eigen::Matrix<Scalar, 1, 3>;
    using Output = ComputeMeshCentroidOutput<Scalar>;

    const auto& vertices = mesh_ref.get_vertices();
    const auto& facets = mesh_ref.get_facets();

    la_runtime_assert(vertices.cols() == 3, "Currently, only 3 dimensions are supported");
    la_runtime_assert(facets.cols() == 3, "Currently, only triangles are supported");

    Vertex3 centroid = Vertex3::Zero();
    Scalar area = 0;

    for (auto facet_id : range_sparse(mesh_ref.get_num_facets(), active_facets)) {
        const Vertex3 v0 = vertices.row(facets(facet_id, 0));
        const Vertex3 v1 = vertices.row(facets(facet_id, 1));
        const Vertex3 v2 = vertices.row(facets(facet_id, 2));
        const Vertex3 tri_centroid = (v0 + v1 + v2) / 3.f;
        const Scalar tri_area = (v1 - v0).cross(v2 - v0).norm() / 2.f;
        centroid += tri_area * tri_centroid;
        area += tri_area;
    } // over triangles
    centroid /= area;

    // Return
    Output output;
    output.area = area;
    output.centroid = centroid;
    return output;
}


} // namespace lagrange

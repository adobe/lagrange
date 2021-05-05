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

#include <exception>

#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>

namespace lagrange {

template <typename MeshType>
auto compute_triangle_normal_const(const MeshType& mesh) -> typename MeshType::AttributeArray
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    using Index = IndexOf<MeshType>;
    using Scalar = ScalarOf<MeshType>;
    using AttributeArray = typename MeshType::AttributeArray;
    using RowVector3s = Eigen::Matrix<Scalar, 1, 3>;

    if (mesh.get_vertex_per_facet() != 3) {
        throw std::runtime_error("Input mesh is not triangle mesh.");
    }
    if (mesh.get_dim() != 3) {
        throw std::runtime_error("Input mesh vertices should have dimension 3.");
    }

    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();
    AttributeArray normals(facets.rows(), 3);

    // Iterate over facets in parallel
    tbb::parallel_for(Index(0), mesh.get_num_facets(), [&](Index f) {
        const RowVector3s p0 = vertices.row(facets(f, 0));
        const RowVector3s p1 = vertices.row(facets(f, 1));
        const RowVector3s p2 = vertices.row(facets(f, 2));
        normals.row(f) = (p1 - p0).cross(p2 - p0).stableNormalized();
    });

    return normals;
}

template <typename MeshType>
void compute_triangle_normal(MeshType& mesh)
{
    mesh.add_facet_attribute("normal");
    mesh.import_facet_attribute("normal", compute_triangle_normal_const(mesh));
}

} // namespace lagrange

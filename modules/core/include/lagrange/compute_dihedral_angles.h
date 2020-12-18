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

#include <lagrange/Edge.h>
#include <lagrange/Logger.h>
#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/attributes/eval_as_attribute.h>
#include <lagrange/compute_triangle_normal.h>

namespace lagrange {
/*
Fills the edge attribute "dihedral_angle" with dihedral angles.
Boundary edges will have value 0.

Requires 3D mesh.

Computes facet normals (mesh facet attribute "normal") and
initializes the mesh edge data if needed.
*/

template <typename MeshType>
void compute_dihedral_angles(MeshType& mesh)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    if (mesh.get_dim() != 3) {
        throw std::runtime_error("Input mesh is not 3D.");
    }

    using Index = typename MeshType::Index;
    using Scalar = typename MeshType::Scalar;

    mesh.initialize_edge_data();

    if (!mesh.has_facet_attribute("normal")) {
        compute_triangle_normal(mesh);
    }

    const auto& facet_normals = mesh.get_facet_attribute("normal");
    const auto& edge_facet_adjacency = mesh.get_edge_facet_adjacency();

    bool non_manifold = false;
    eval_as_edge_attribute(mesh, "dihedral_angle", [&](Index i) -> Scalar {
        const auto& adj_facets = edge_facet_adjacency[i];
        if (adj_facets.size() <= 1) {
            return 0;
        } else {
            const Eigen::Matrix<Scalar, 3, 1>& n1 = facet_normals.row(adj_facets[0]);
            const Eigen::Matrix<Scalar, 3, 1>& n2 = facet_normals.row(adj_facets[1]);
            return std::atan2((n1.cross(n2)).norm(), n1.dot(n2));
        }
        if (adj_facets.size() > 2) {
            non_manifold = true;
        }
    });
    if (non_manifold) {
        lagrange::logger().warn("Computing dihedral angles on a non-manifold mesh!");
    }
}
} // namespace lagrange

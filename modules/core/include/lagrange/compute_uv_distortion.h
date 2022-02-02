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

#include <cassert>
#include <iostream>

#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/attributes/attribute_utils.h>
#include <lagrange/common.h>
#include <lagrange/utils/safe_cast.h>

namespace lagrange {

/**
 * This function computes the distortion of mapping from 3D triangular mesh
 * to UV domain.  Let f : R^2 -> R^2 represent the mapping, and let F =
 * grad(f) be the deformation gradient.  The distortion measure is defined
 * as |F|^2 / det(F), where |F| is the Frobenius norm of the matrix and
 * det(F) is its determinant.
 *
 * See Conformal AMIPS 2D energy:
 * https://cs.nyu.edu/~panozzo/papers/SLIM2017.pdf.
 */
template <typename MeshType>
void compute_uv_distortion(MeshType& mesh)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    if (mesh.get_vertex_per_facet() != 3) {
        throw std::runtime_error("Input mesh must be a triangle mesh.");
    }

    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using AttributeArray = typename MeshType::AttributeArray;

    if (mesh.has_vertex_attribute("uv") && !mesh.has_corner_attribute("uv")) {
        map_vertex_attribute_to_corner_attribute(mesh, "uv");
    }
    if (!mesh.has_corner_attribute("uv")) {
        throw std::runtime_error("UV vertex attribute is missing.");
    }

    //        const Index num_vertices = mesh->get_num_vertices();
    const Index num_facets = mesh.get_num_facets();
    const Index vertex_per_facet = mesh.get_vertex_per_facet();
    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();

    const auto& uv = mesh.get_corner_attribute("uv");
    la_runtime_assert(safe_cast<Index>(uv.rows()) == num_facets * vertex_per_facet);

    const Scalar INVALID = std::numeric_limits<Scalar>::max();

    AttributeArray distortion(num_facets, 1);
    distortion.setConstant(INVALID);

    for (Index i = 0; i < num_facets; i++) {
        Eigen::Matrix<Index, 1, 3> f = facets.row(i);
        Eigen::Matrix<Scalar, 1, 3> v0, v1, v2;
        Eigen::Matrix<Scalar, 1, 2> V0, V1, V2;
        v0 << vertices.row(f[0]);
        v1 << vertices.row(f[1]);
        v2 << vertices.row(f[2]);
        V0 << uv.row(i * 3 + 0);
        V1 << uv.row(i * 3 + 1);
        V2 << uv.row(i * 3 + 2);

        if (V0.minCoeff() < 0.0 || V0.maxCoeff() > 1.0) continue;
        if (V1.minCoeff() < 0.0 || V1.maxCoeff() > 1.0) continue;
        if (V2.minCoeff() < 0.0 || V2.maxCoeff() > 1.0) continue;

        // AMIPS energy can be computed geometrically.
        // Area ratio == det(F)
        // Dirichlet energy == |F|^2
        const Eigen::Matrix<Scalar, 1, 3> e0 = v1 - v2;
        const Eigen::Matrix<Scalar, 1, 3> e1 = v2 - v0;
        const Eigen::Matrix<Scalar, 1, 3> e2 = v0 - v1;
        const Scalar l0 = e0.norm();
        const Scalar l1 = e1.norm();
        const Scalar l2 = e2.norm();
        const Scalar s = 0.5f * (l0 + l1 + l2);
        const Scalar a = sqrt(s * (s - l0) * (s - l1) * (s - l2));

        const Eigen::Matrix<Scalar, 1, 2> E0 = V1 - V2;
        const Eigen::Matrix<Scalar, 1, 2> E1 = V2 - V0;
        const Eigen::Matrix<Scalar, 1, 2> E2 = V0 - V1;
        const Scalar L0 = E0.norm();
        const Scalar L1 = E1.norm();
        const Scalar L2 = E2.norm();
        const Scalar S = 0.5f * (L0 + L1 + L2);
        const Scalar A = sqrt(S * (S - L0) * (S - L1) * (S - L2));

        const Scalar area_ratio = A / a;

        const Scalar cot_0 = e1.dot(-e2) / e1.cross(-e2).norm();
        const Scalar cot_1 = e2.dot(-e0) / e2.cross(-e0).norm();
        const Scalar cot_2 = e0.dot(-e1) / e0.cross(-e1).norm();

        // Need to divid by the area since we want energy density.
        const Scalar dirichlet = 0.5f * (cot_0 * L0 * L0 + cot_1 * L1 * L1 + cot_2 * L2 * L2) / a;

        // Distortion measure is bounded from below by 2.  It is achieved if
        // and only if the mapping f is isometric.
        distortion(i, 0) = dirichlet / area_ratio;
    }

    mesh.add_facet_attribute("distortion");
    mesh.set_facet_attribute("distortion", distortion);
}
} // namespace lagrange

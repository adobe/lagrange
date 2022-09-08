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

#include <lagrange/Mesh.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/internal/bfs_orient.h>

#include <Eigen/Core>

namespace lagrange {

///
/// Orient the facets of a mesh so that the signed volume of each connected component is positive or negative.
///
/// @param[in,out] mesh         Mesh whose facets needs to be re-oriented.
/// @param[in]     positive     Orient to have positive signed volume.
///
/// @tparam        VertexArray  Type of vertex array.
/// @tparam        FacetArray   Type of facet array.
///
template <typename VertexArray, typename FacetArray>
void orient_outward(lagrange::Mesh<VertexArray, FacetArray>& mesh, bool positive = true)
{
    using Scalar = typename VertexArray::Scalar;
    using Index = typename FacetArray::Scalar;
    using RowVector3r = Eigen::Matrix<Scalar, 1, 3>;

    if (mesh.get_num_facets() == 0) {
        // TODO: Fix igl::bfs_orient to work on empty meshes.
        return;
    }

    auto tetra_signed_volume = [](const RowVector3r& p1,
                                  const RowVector3r& p2,
                                  const RowVector3r& p3,
                                  const RowVector3r& p4) {
        return (p2 - p1).dot((p3 - p1).cross(p4 - p1)) / 6.0;
    };

    auto component_signed_volumes = [&](const auto& vertices,
                                        const auto& facets,
                                        const auto& components,
                                        auto& signed_volumes) {
        la_runtime_assert(vertices.cols() == 3);
        la_runtime_assert(facets.cols() == 3);
        std::array<RowVector3r, 4> t;
        t[3] = RowVector3r::Zero(vertices.cols());
        signed_volumes.resize(components.maxCoeff() + 1);
        signed_volumes.setZero();
        for (Eigen::Index f = 0; f < facets.rows(); ++f) {
            for (Eigen::Index lv = 0; lv < facets.cols(); ++lv) {
                t[lv] = vertices.row(facets(f, lv));
            }
            double vol = tetra_signed_volume(t[0], t[1], t[2], t[3]);
            signed_volumes(components(f)) -= vol;
        }
    };

    const auto& vertices = mesh.get_vertices();
    FacetArray facets;
    mesh.export_facets(facets);

    // Orient connected components on the surface
    Eigen::Matrix<Index, Eigen::Dynamic, 1> components;
    lagrange::internal::bfs_orient(facets, facets, components);

    // Signed volumes per components
    Eigen::Matrix<Scalar, Eigen::Dynamic, 1> signed_volumes;
    component_signed_volumes(vertices, facets, components, signed_volumes);

    // Flip facets in each compoenent with incorrect volume sign
    for (Eigen::Index f = 0; f < facets.rows(); ++f) {
        if ((positive ? 1 : -1) * signed_volumes(components(f)) < 0) {
            facets.row(f) = facets.row(f).reverse().eval();
        }
    }

    mesh.import_facets(facets);
}

} // namespace lagrange

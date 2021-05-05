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
#include <exception>
#include <iostream>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <igl/per_vertex_normals.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <lagrange/common.h>
#include <lagrange/Mesh.h>
#include <lagrange/compute_triangle_normal.h>
#include <lagrange/MeshTrait.h>

namespace lagrange {
template <typename MeshType>
void compute_vertex_normal(
    MeshType& mesh,
    const igl::PerVertexNormalsWeightingType weighting =
        igl::PER_VERTEX_NORMALS_WEIGHTING_TYPE_ANGLE)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    using Index = IndexOf<MeshType>;
    using Scalar = ScalarOf<MeshType>;

    if (mesh.get_vertex_per_facet() != 3) {
        throw std::runtime_error("Input mesh is not triangle mesh.");
    }

    if (!mesh.has_facet_attribute("normal")) {
        compute_triangle_normal(mesh);
        LA_ASSERT(mesh.has_facet_attribute("normal"));
    }

    using AttributeArray = typename MeshType::AttributeArray;
    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();
    const auto& facet_normals = mesh.get_facet_attribute("normal");
    AttributeArray vertex_normals(vertices.rows(), 3);
    vertex_normals.setZero();

    {
        // Reimplementation of igl::per_vertex_normals, except we use a parallel sum over the
        // vertices if mesh edge data is available.
        AttributeArray weights(facets.rows(), 3);

        switch (weighting) {
        case igl::PER_VERTEX_NORMALS_WEIGHTING_TYPE_UNIFORM: weights.setConstant(Scalar(1)); break;
        case igl::PER_VERTEX_NORMALS_WEIGHTING_TYPE_DEFAULT:
        case igl::PER_VERTEX_NORMALS_WEIGHTING_TYPE_AREA: {
            Eigen::Matrix<Scalar, Eigen::Dynamic, 1> areas;
            igl::doublearea(vertices, facets, areas);
            weights = areas.replicate(1, 3);
            break;
        }
        case igl::PER_VERTEX_NORMALS_WEIGHTING_TYPE_ANGLE:
            igl::internal_angles(vertices, facets, weights);
            break;
        default: assert(false && "Unknown weighting type");
        }

        if (mesh.is_edge_data_initialized_new()) {
            // Parallel version, iterating over vertices
            tbb::parallel_for(Index(0), mesh.get_num_vertices(), [&](Index v) {
                mesh.foreach_corners_around_vertex_new(v, [&](const Index c) {
                    const Index f = c / 3;
                    const Index lv = c % 3;
                    vertex_normals.row(v) += weights(f, lv) * facet_normals.row(f);
                });
            });
        } else {
            // Loop over faces
            for (Index f = 0; f < static_cast<Index>(facets.rows()); ++f) {
                // Throw normal at each corner
                for (Index lv = 0; lv < 3; ++lv) {
                    const Index v = facets(f, lv);
                    vertex_normals.row(v) += weights(f, lv) * facet_normals.row(f);
                }
            }
        }
        // Take average via normalization.
        //
        // Note: For some reason stableNormalize() is not yet available as a vectorwise operation,
        // so we cannot simply call `vertex_normals.rowwise().stableNormalize()` and call it a day.
        tbb::parallel_for(Index(0), mesh.get_num_vertices(), [&](Index v) {
            vertex_normals.row(v).stableNormalize();
        });
    }

    mesh.add_vertex_attribute("normal");
    mesh.import_vertex_attribute("normal", vertex_normals);
}

} // namespace lagrange

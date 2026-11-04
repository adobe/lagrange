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

#include <lagrange/MeshTrait.h>
#include <lagrange/common.h>
#include <lagrange/legacy/inline.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/safe_cast.h>

#include <algorithm> /* std::lower_bound */
#include <cmath> /* std::abs */
#include <iterator> /* std::distance */
#include <random> /* std::uniform_real_distribution */

namespace lagrange {
namespace sampling {
LAGRANGE_LEGACY_INLINE
namespace legacy {

template <typename MeshType, typename Rng>
auto random_sample_uniform(
    const MeshType& mesh,
    size_t num_samples,
    Rng&& rng,
    const bool compute_facet_indices = false)
    -> std::tuple<
        typename MeshType::VertexArray::PlainObject,
        Eigen::Matrix<typename MeshType::Index, Eigen::Dynamic, 1>>
{
    static_assert(lagrange::MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    using VertexArray = typename MeshType::VertexArray::PlainObject;
    using AttributeArray = typename MeshType::AttributeArray;
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using IndexArray = Eigen::Matrix<typename MeshType::Index, Eigen::Dynamic, 1>;

    la_runtime_assert(mesh.get_vertex_per_facet() == 3); // only works on triangular mesh
    la_runtime_assert(mesh.has_facet_attribute("area")); // need triangle areas to proceed
    const auto& areas = mesh.get_facet_attribute("area");
    la_runtime_assert(areas.minCoeff() > 0); // all triangles must have positive non null area

    AttributeArray cumulative_areas(areas.rows(), 1);
    Scalar accum = 0;
    for (Index kk = 0, ee = static_cast<Index>(areas.rows()); kk < ee; kk++) {
        accum += areas(kk);
        cumulative_areas(kk) = accum;
    }

    la_runtime_assert(accum > 0); // mesh must have positive non null area

    std::uniform_real_distribution<Scalar> dist_cumulative(0, accum);
    std::uniform_real_distribution<Scalar> dist_triangle(0, 1);

    const auto& triangles = mesh.get_facets();
    const auto& vertices = mesh.get_vertices();
    const auto dim = mesh.get_dim();
    VertexArray points(num_samples, dim);
    auto indices = compute_facet_indices ? IndexArray(num_samples) : IndexArray();
    for (Index kk = 0; kk < static_cast<Index>(num_samples); kk++) {
        const auto triangle_selection = dist_cumulative(rng);
        const auto triangle_index = std::distance(
            cumulative_areas.data(),
            std::lower_bound(
                cumulative_areas.data(),
                cumulative_areas.data() + cumulative_areas.size(),
                triangle_selection));
        assert(safe_cast<Index>(triangle_index) < mesh.get_num_facets());
        const auto& triangle = triangles.row(triangle_index);

        const auto va = vertices.row(triangle(0));
        const auto vb = vertices.row(triangle(1));
        const auto vc = vertices.row(triangle(2));

        auto alpha = dist_triangle(rng);
        auto beta = dist_triangle(rng);
        if (alpha + beta > 1) {
            alpha = 1 - alpha;
            beta = 1 - beta;
        }
        assert(alpha + beta <= 1);

        points.row(kk) = va + alpha * (vb - va) + beta * (vc - va);
        if (compute_facet_indices) indices(kk) = static_cast<Index>(triangle_index);
    }

    return std::make_tuple(points, indices);
}

} // namespace legacy
} // namespace sampling
} // namespace lagrange

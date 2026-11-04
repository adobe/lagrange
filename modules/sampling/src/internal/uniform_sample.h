/*
 * Copyright 2023 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/utils/assert.h>
#include <lagrange/utils/safe_cast.h>

#include <Eigen/Core>

#include <algorithm> /* std::lower_bound */
#include <cmath> /* std::abs */
#include <iterator> /* std::distance */
#include <random> /* std::uniform_real_distribution */

namespace lagrange::sampling {

template <
    typename VertexArray,
    typename FacetArray,
    typename AreaArray,
    typename Rng,
    typename SampleArray,
    typename IndexArray>
void uniform_sample(
    const Eigen::MatrixBase<VertexArray>& vertices,
    const Eigen::MatrixBase<FacetArray>& triangles,
    const Eigen::MatrixBase<AreaArray>& areas,
    const size_t number_of_sample,
    Rng&& rng,
    Eigen::MatrixBase<SampleArray>& samples,
    Eigen::MatrixBase<IndexArray>& sample_facet_indices,
    bool compute_facet_indices = false)
{
    using Scalar = typename VertexArray::Scalar;
    using Index = typename FacetArray::Scalar;

    static_assert(std::is_same_v<Scalar, typename AreaArray::Scalar>);
    static_assert(std::is_same_v<Scalar, typename SampleArray::Scalar>);
    static_assert(std::is_same_v<Index, typename IndexArray::Scalar>);

    la_runtime_assert(triangles.cols() == 3); // only works on triangular mesh
    la_runtime_assert(areas.minCoeff() >= 0); // all triangles must have positive non null area
    la_runtime_assert(areas.rows() == triangles.rows());

    Eigen::Matrix<Scalar, Eigen::Dynamic, 1> cumulative_areas(areas.rows(), 1);
    Scalar accum = 0;
    for (Index kk = 0, ee = areas.rows(); kk < ee; kk++) {
        accum += areas(kk);
        cumulative_areas(kk) = accum;
    }

    la_runtime_assert(accum > 0); // mesh must have positive non null area

    std::uniform_real_distribution<Scalar> dist_cumulative(0, accum);
    std::uniform_real_distribution<Scalar> dist_triangle(0, 1);

    const auto dim = vertices.cols();
    samples.resize(number_of_sample, dim);
    if (compute_facet_indices) {
        sample_facet_indices.resize(number_of_sample, 1);
    }
    for (Index kk = 0; kk < number_of_sample; kk++) {
        const auto triangle_selection = dist_cumulative(rng);
        const auto triangle_index = std::distance(
            cumulative_areas.data(),
            std::lower_bound(
                cumulative_areas.data(),
                cumulative_areas.data() + cumulative_areas.size(),
                triangle_selection));
        la_debug_assert(safe_cast<Index>(triangle_index) < safe_cast<Index>(triangles.rows()));
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
        la_debug_assert(alpha + beta <= 1);

        samples.row(kk) = va + alpha * (vb - va) + beta * (vc - va);
        if (compute_facet_indices) sample_facet_indices(kk) = triangle_index;
    }
}

} // namespace lagrange::sampling

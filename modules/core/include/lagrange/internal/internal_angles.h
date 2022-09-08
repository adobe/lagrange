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
#pragma once

#include <lagrange/utils/assert.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <Eigen/Dense>

namespace lagrange::internal {

///
/// Compute internal angles for a triangle mesh.
///
/// @param[in]  vertices  #vertices by dim Eigen matrix of mesh vertex nD positions.
/// @param[in]  facets    #facets by 3 eigen Matrix of face (triangle) indices,
/// @param[out] angles    #facets by 3 eigen Matrix of internal angles for triangles, columns
///                       correspond to edges [1,2],[2,0],[0,1].
///
/// @tparam     DerivedV  Vertices matrix type.
/// @tparam     DerivedF  Facets matrix type.
/// @tparam     DerivedK  Angles matrix type.
///
template <typename DerivedV, typename DerivedF, typename DerivedK>
void internal_angles(
    const Eigen::MatrixBase<DerivedV>& vertices,
    const Eigen::MatrixBase<DerivedF>& facets,
    Eigen::PlainObjectBase<DerivedK>& angles)
{
    la_runtime_assert(facets.cols() == 3);
    // Computing Cross-Products and Rotations in 2- and 3-Dimensional Euclidean Spaces [Kahan 2016]
    // Formula can be found in §13 (page 15)
    using Scalar = typename DerivedV::Scalar;
    using Index = typename DerivedF::Index;
    const Index m = facets.rows();
    angles.resize(m, 3);
    tbb::parallel_for(Index(0), m, [&](const Index f) {
        for (size_t d = 0; d < 3; d++) {
            auto v1 = vertices.row(facets(f, d)) - vertices.row(facets(f, (d + 1) % 3));
            auto v2 = vertices.row(facets(f, d)) - vertices.row(facets(f, (d + 2) % 3));
            angles(f, d) = Scalar(2) * std::atan2(
                                           (v1.stableNormalized() - v2.stableNormalized()).norm(),
                                           (v1.stableNormalized() + v2.stableNormalized()).norm());
        }
    });
}

} // namespace lagrange::internal

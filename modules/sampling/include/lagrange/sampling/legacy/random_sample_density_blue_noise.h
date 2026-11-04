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

#include <lagrange/sampling/DensityStatus.h>

#include <lagrange/MeshTrait.h>
#include <lagrange/common.h>
#include <lagrange/legacy/inline.h>

#include <tuple>

namespace lagrange {
namespace sampling {
LAGRANGE_LEGACY_INLINE
namespace legacy {

///
/// This function sample a triangular mesh according to the target density described by
/// density_callback. This is achieved by first generating a large uniformally sampled point cloud,
/// which is then decimated incrementally using a total weight minimizing strategy.
/// For more detail, please see [1].
/// Choosing oversample_ratio as twice rmax_factor works well for most cases.
///
/// density_callback takes two parameters:
/// - first points where the density should evaluated,
/// - second a array of closest triangle indices in the input facet list.
/// It should return an array with one column and the same number of rows as the first argument.
/// Densities are valid when they meet the following conditions:
/// - does not contains negative or non finite value,
/// - at least one value is not null.
///
/// @return (status, cloud, densities, indices) where
/// - status reports density validity,
/// - cloud is the sampled point cloud and
/// - densities is density attribute, avaluated on cloud.
/// - indices are original facet indices of sampled points
/// If status is not valid then cloud, densities and indices are empty.
///
/// [1] Yuksel, C. (2015, May). Sample elimination for generating Poisson disk sample sets. In
/// Computer Graphics Forum (Vol. 34, No. 2, pp. 25-32).
///
template <typename MeshType, typename DensityCallback, typename Rng>
auto random_sample_density_blue_noise(
    const MeshType& mesh /**< [in] Input triangular mesh with facet area attribute.*/,
    const typename MeshType::Index number_of_sample /**< [in] Number of sample in the output cloud.*/,
    DensityCallback density_callback /**< [in] Density description function.*/,
    Rng&& rng /**< [in] Standard number generator.*/,
    const double bias = 0 /**< [in] Bias value between 0 (prescribed density) and 1 (uniform density). Increase to avoid clumping at the cost of loosing contrast.*/,
    const double oversample_ratio = 8 /**< [in] Oversampling ratio. Increase along with rmax_factor to avoid clumping at the cost of increased computation time.*/,
    const double rmax_factor = 4 /**< [in] Neigborbood size ratio during weighs computation. Increase along with oversample_ratio to avoid clumping at the cost of increase computation time.*/)
    -> std::tuple<DensityStatus, typename MeshType::VertexArray::PlainObject, typename MeshType::AttributeArray, Eigen::Matrix<typename MeshType::Index, Eigen::Dynamic, 1>>;

} // namespace legacy
} // namespace sampling
} // namespace lagrange

#include <lagrange/sampling/legacy/random_sample_density_blue_noise.impl.h>

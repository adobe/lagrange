/*
 * Copyright 2026 Adobe. All rights reserved.
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

#include <lagrange/SurfaceMesh.h>
#include <lagrange/sampling/DensityStatus.h>
#include <lagrange/utils/span.h>

#include <functional>
#include <string_view>
#include <variant>
#include <vector>

namespace lagrange {
namespace sampling {

///
/// @addtogroup module-sampling
/// @{
///

///
/// Option struct for blue noise sampling with a prescribed density.
///
/// The density field controls how sample density is determined:
/// - `std::monostate` (default): Uniform density across the mesh.
/// - `std::string_view`: Name of a per-facet or per-vertex mesh attribute whose value type must
///   match the mesh `Scalar` type. Per-vertex values are interpolated at sample positions using
///   barycentric coordinates.
/// - `DensityCallback`: User-provided callback function for custom density evaluation.
///
/// @tparam Scalar  Mesh scalar type.
/// @tparam Index   Mesh index type.
///
/// @see sample_blue_noise
///
template <typename Scalar, typename Index>
struct BlueNoiseSamplingOptions
{
    /// Density evaluation callback type. Takes (positions, facet_indices, output_densities) where
    /// positions is a flat span of size `num_points * dimension`, facet_indices is a span of size
    /// `num_points`, and output_densities is a span of size `num_points` to be filled.
    using DensityCallback = std::function<void(
        span<const Scalar> positions,
        span<const Index> facet_indices,
        span<Scalar> output_densities)>;

    /// Random number generator seed.
    size_t seed = 0;

    /// Number of desired output samples. Must be greater than zero.
    size_t num_samples = 0;

    /// Bias value between 0 (prescribed density) and 1 (uniform density).
    /// Increase to avoid clumping at the cost of losing density contrast.
    double bias = 0.0;

    /// Oversampling ratio.
    /// Increase along with rmax_factor to avoid clumping at the cost of increased computation time.
    double oversample_ratio = 8.0;

    /// Neighborhood size ratio during weight computation. Increase along with oversample_ratio to
    /// avoid clumping at the cost of increased computation time.
    double rmax_factor = 4.0;

    /// Density specification.
    /// - `std::monostate`: Constant (uniform) density.
    /// - `std::string_view`: Name of a per-facet or per-vertex scalar mesh attribute. The
    ///   attribute's value type must match the mesh `Scalar` type.
    /// - `DensityCallback`: User-defined density evaluation callback.
    std::variant<std::monostate, std::string_view, DensityCallback> density;
};

///
/// Result struct for blue noise sampling.
///
/// @tparam Scalar  Mesh scalar type.
/// @tparam Index   Mesh index type.
///
template <typename Scalar, typename Index>
struct BlueNoiseSamplingResult
{
    /// Output sample positions, stored as a flat array of size `num_samples * dimension`.
    std::vector<Scalar> samples;

    /// Output density values evaluated at each sample, stored as an array of size `num_samples`.
    std::vector<Scalar> densities;

    /// Output facet indices for each sample, stored as an array of size `num_samples`.
    std::vector<Index> facet_indices;
};

///
/// Sample a triangle mesh using blue noise (Poisson disk) sampling.
///
/// This is achieved by first generating a large uniformly sampled point cloud, which is then
/// decimated incrementally using a total weight minimizing strategy. For more detail, please see:
///
/// > Yuksel, C. (2015, May). Sample elimination for generating Poisson disk sample sets.
/// > In Computer Graphics Forum (Vol. 34, No. 2, pp. 25-32).
///
/// The density mode is controlled by `options.density`:
/// - **Uniform**: `std::monostate` (default) — all samples receive equal density.
/// - **Attribute**: `std::string_view` — density is read from a named per-facet or per-vertex
///   scalar attribute. Per-vertex values are interpolated at sample positions using barycentric
///   coordinates.
/// - **Callback**: `DensityCallback` — a user-provided function evaluates density at each sample
///   position.
///
/// Densities must not contain negative or non-finite values, and at least one value
/// must be non-zero. Only the relative magnitude of the density field is used for distributing the
/// samples. Use @ref sample_epsilon if epsilon-coverage property is desired.
///
/// @param[in]  mesh     Input triangle mesh.
/// @param[in]  options  Sampling options (including density specification).
///
/// @tparam     Scalar   Mesh scalar type.
/// @tparam     Index    Mesh index type.
///
/// @return     A BlueNoiseSamplingResult containing the sampled points, their densities, and their
///             facet indices.
///
/// @throws     DensityError if the evaluated density contains invalid values (non-finite, negative,
///             or zero sum).
///
/// @see        BlueNoiseSamplingOptions, BlueNoiseSamplingResult, DensityError, sample_epsilon
///
template <typename Scalar, typename Index>
BlueNoiseSamplingResult<Scalar, Index> sample_blue_noise(
    const SurfaceMesh<Scalar, Index>& mesh,
    const BlueNoiseSamplingOptions<Scalar, Index>& options = {});

/// @}

} // namespace sampling
} // namespace lagrange

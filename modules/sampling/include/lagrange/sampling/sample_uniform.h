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
#include <lagrange/utils/span.h>

#include <vector>

namespace lagrange {
namespace sampling {

///
/// @addtogroup module-sampling
/// @{
///

///
/// Option struct for uniform sampling.
///
/// @see sample_uniform
///
struct UniformSamplingOptions
{
    /// Random number generator seed.
    size_t seed = 0;

    /// Number of desired output samples.
    size_t num_samples = 0;
};

/// @deprecated Use UniformSamplingOptions instead.
using SamplingOptions [[deprecated("Use UniformSamplingOptions instead.")]] =
    UniformSamplingOptions;

///
/// Result struct for uniform sampling.
///
/// @tparam Scalar  Mesh scalar type.
/// @tparam Index   Mesh index type.
///
template <typename Scalar, typename Index>
struct UniformSamplingResult
{
    /// Output sample positions, stored as a flat array of size `num_samples * dimension`.
    std::vector<Scalar> samples;

    /// Output facet indices for each sample, stored as an array of size `num_samples`.
    std::vector<Index> facet_indices;
};

///
/// Uniformly sample a triangle mesh, writing results into caller-provided buffers.
///
/// @param[in]  mesh                  Input triangle mesh.
/// @param[in]  num_samples           Number of samples.
/// @param[out] sample_positions      Output buffer for sample positions (size = num_samples * mesh.get_dimension()).
/// @param[out] sample_facet_indices  Output buffer for facet indices of each sample.
/// @param[in]  options               Sampling options (seed is used; if options.num_samples is non-zero it must equal num_samples).
///
/// @tparam     Scalar                Scalar type for vertex positions.
/// @tparam     Index                 Index type for indices.
///
/// @deprecated Use the overload taking UniformSamplingOptions (without explicit num_samples) instead.
///
template <typename Scalar, typename Index>
[[deprecated("Use the overload taking UniformSamplingOptions instead.")]]
void sample_uniform(
    const SurfaceMesh<Scalar, Index>& mesh,
    size_t num_samples,
    span<Scalar> sample_positions,
    span<Index> sample_facet_indices = {},
    const UniformSamplingOptions& options = {});

///
/// Uniformly sample a triangle mesh, returning results in a struct.
///
/// @param[in]  mesh     Input triangle mesh.
/// @param[in]  options  Sampling options.
///
/// @tparam     Scalar   Scalar type for vertex positions.
/// @tparam     Index    Index type for indices.
///
/// @return     A UniformSamplingResult containing the sampled points and their facet indices.
///
/// @see        UniformSamplingResult, UniformSamplingOptions
///
template <typename Scalar, typename Index>
UniformSamplingResult<Scalar, Index> sample_uniform(
    const SurfaceMesh<Scalar, Index>& mesh,
    const UniformSamplingOptions& options = {});

/// @}

} // namespace sampling
} // namespace lagrange

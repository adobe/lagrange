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

#include <cstddef>
#include <string_view>
#include <vector>

namespace lagrange {
namespace sampling {

///
/// @addtogroup module-sampling
/// @{
///

///
/// Option struct for epsilon sampling.
///
/// @see sample_epsilon
///
struct EpsilonSamplingOptions
{
    /// Random number generator seed.
    size_t seed = 0;

    /// Number of consecutive rejected darts after which the sampling terminates. Larger values
    /// produce a more nearly maximal (better covered) sampling at the cost of more computation.
    size_t max_failed_attempts = 1000;

    /// Whether to also output barycentric coordinates for each sample.
    bool compute_barycentric = false;

    /// Whether to check if every facet-reference vertex is epsilon-covered by a sample. If not,
    /// add this vertex to samples.
    bool ensure_vertex_coverage = false;
};

///
/// Result struct for epsilon sampling.
///
/// @tparam Scalar  Mesh scalar type.
/// @tparam Index   Mesh index type.
///
template <typename Scalar, typename Index>
struct EpsilonSamplingResult
{
    /// Output sample positions, stored as a flat array of size `num_samples * dimension`.
    std::vector<Scalar> samples;

    /// Output facet indices for each sample, stored as an array of size `num_samples`.
    std::vector<Index> facet_indices;

    /// Output barycentric coordinates for each sample, stored as a flat array of size
    /// `num_samples * 3`. Only filled when `EpsilonSamplingOptions::compute_barycentric` is true.
    std::vector<Scalar> barycentric;
};

///
/// Generate an epsilon sampling of a triangle mesh graded by a local feature size (LFS) field.
///
/// Given a per-vertex LFS scalar attribute and a constant `epsilon`, this produces a set of surface
/// samples aiming for every point on the surface to lie within `epsilon * lfs` of some sample (an
/// epsilon-sample in the Amenta-Bern sense). Sampling is denser where LFS is small (thin or
/// high-curvature regions) and sparser where LFS is large. The total number of samples is not
/// specified; it emerges from `epsilon` and the LFS field.
///
/// @note The sampling is computed by variable-radius Poisson-disk (dart throwing): darts are
/// generated uniformly, and a dart at position `x` is accepted only if no existing sample lies
/// within `epsilon * lfs(x)` of it. Sampling terminates after `options.max_failed_attempts`
/// consecutive rejected darts, at which point the sampling is probabilistically (not strictly)
/// maximal: full epsilon-coverage is not guaranteed, and larger `max_failed_attempts` values make
/// it more likely.
///
/// @note Unlike @ref sample_blue_noise, this method does not take a target number of
/// samples. The number of samples emerges from `epsilon` and the LFS field, and the result carries
/// the theoretical epsilon-coverage guarantee described above (Amenta-Bern).
///
/// @param[in]  mesh           Input triangle mesh.
/// @param[in]  lfs_attribute  Name of a per-vertex, single-channel scalar attribute holding the
///                            local feature size. Its value type must match the mesh `Scalar` type,
///                            and all values must be strictly positive. See
///                            @ref lagrange::raycasting::compute_local_feature_size for one way to
///                            compute this attribute.
/// @param[in]  epsilon        Sampling density constant. Must be strictly positive.
/// @param[in]  options        Sampling options.
///
/// @tparam     Scalar         Mesh scalar type.
/// @tparam     Index          Mesh index type.
///
/// @return     An EpsilonSamplingResult containing the sampled points and their facet indices (and
///             barycentric coordinates if requested).
///
/// @see        EpsilonSamplingOptions, EpsilonSamplingResult, sample_blue_noise
///
/// > Amenta, Nina, and Marshall Bern. "Surface reconstruction by Voronoi filtering."
/// > Proceedings of the fourteenth annual symposium on Computational geometry. 1998.
///
template <typename Scalar, typename Index>
EpsilonSamplingResult<Scalar, Index> sample_epsilon(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::string_view lfs_attribute,
    Scalar epsilon,
    const EpsilonSamplingOptions& options = {});

/// @}

} // namespace sampling
} // namespace lagrange

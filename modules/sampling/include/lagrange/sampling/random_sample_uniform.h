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

// This header is kept for backward compatibility.
// New code should include <lagrange/sampling/sample_uniform.h> directly.

#ifdef LAGRANGE_ENABLE_LEGACY_HEADERS
    #include <lagrange/sampling/legacy/random_sample_uniform.h>
#endif

#include <lagrange/sampling/sample_uniform.h>
#include <lagrange/utils/warning.h>

namespace lagrange {
namespace sampling {

///
/// @addtogroup module-sampling
/// @{
///

/// @deprecated Use sample_uniform(mesh, UniformSamplingOptions) instead.
template <typename Scalar, typename Index>
[[deprecated("Use sample_uniform(mesh, UniformSamplingOptions) instead.")]]
void random_sample_uniform(
    const SurfaceMesh<Scalar, Index>& mesh,
    size_t num_samples,
    span<Scalar> sample_positions,
    span<Index> sample_facet_indices = {},
    const UniformSamplingOptions& options = {})
{
    LA_IGNORE_DEPRECATION_WARNING_BEGIN
    sample_uniform(mesh, num_samples, sample_positions, sample_facet_indices, options);
    LA_IGNORE_DEPRECATION_WARNING_END
}

/// @deprecated Use sample_uniform instead.
template <typename Scalar, typename Index>
[[deprecated("Use sample_uniform instead.")]]
UniformSamplingResult<Scalar, Index> random_sample_uniform(
    const SurfaceMesh<Scalar, Index>& mesh,
    const UniformSamplingOptions& options = {})
{
    return sample_uniform(mesh, options);
}

/// @}

} // namespace sampling
} // namespace lagrange

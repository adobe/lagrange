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

// This header is kept for backward compatibility.
// New code should include <lagrange/sampling/sample_blue_noise.h> directly.

#include <lagrange/sampling/sample_blue_noise.h>

namespace lagrange {
namespace sampling {

///
/// @addtogroup module-sampling
/// @{
///

/// @deprecated Use sample_blue_noise instead.
template <typename Scalar, typename Index>
[[deprecated("Use sample_blue_noise instead.")]]
BlueNoiseSamplingResult<Scalar, Index> random_sample_blue_noise(
    const SurfaceMesh<Scalar, Index>& mesh,
    const BlueNoiseSamplingOptions<Scalar, Index>& options = {})
{
    return sample_blue_noise(mesh, options);
}

/// @}

} // namespace sampling
} // namespace lagrange

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

namespace lagrange {
namespace sampling {

///
/// @addtogroup module-sampling
/// @{
///

/// Result of validating a per-sample density array used for blue noise sampling.
///
/// Blue noise sampling requires a density array where every entry is finite, non-negative, and the
/// total sum is strictly positive. This enum reports which condition (if any) is violated.
enum struct DensityStatus {
    Valid = 0, ///< Density is valid.
    ContainsNonFiniteOrNan = 1, ///< Density contains non-finite or NaN values.
    ContainsNegative = 2, ///< Density contains negative values.
    HasNonPositiveSum = 3, ///< Density has a zero or negative sum.
    HasZeroSum [[deprecated("Use HasNonPositiveSum instead")]] =
        HasNonPositiveSum, /// @deprecated Use HasNonPositiveSum instead.
};

/// @}

} // namespace sampling
} // namespace lagrange

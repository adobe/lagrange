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
#include <lagrange/sampling/DensityError.h>

namespace lagrange::sampling {

namespace {

const char* to_string(DensityStatus status)
{
    switch (status) {
    case DensityStatus::ContainsNonFiniteOrNan: return "Density contains non-finite or NaN values.";
    case DensityStatus::ContainsNegative: return "Density contains negative values.";
    case DensityStatus::HasNonPositiveSum: return "Density has a zero or negative sum.";
    case DensityStatus::Valid: return "Density is valid.";
    default: return "Unknown density status.";
    }
}

} // namespace

DensityError::DensityError(DensityStatus status)
    : Error(to_string(status))
    , m_status(status)
{}

DensityError::~DensityError() = default;

} // namespace lagrange::sampling

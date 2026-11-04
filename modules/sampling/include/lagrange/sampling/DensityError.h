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

#include <lagrange/sampling/DensityStatus.h>
#include <lagrange/sampling/api.h>
#include <lagrange/utils/Error.h>

namespace lagrange {
namespace sampling {

///
/// @addtogroup module-sampling
/// @{
///

///
/// An exception thrown when density validation fails during blue noise sampling.
///
/// Provides access to the specific DensityStatus error code that triggered the exception.
///
struct LA_SAMPLING_API DensityError : public lagrange::Error
{
    ///
    /// Construct a DensityError from a DensityStatus code.
    ///
    /// @param[in] status  The density validation status (must not be DensityStatus::Valid).
    ///
    explicit DensityError(DensityStatus status);

    ~DensityError() override;

    ///
    /// Returns the DensityStatus error code that caused this exception.
    ///
    DensityStatus status() const noexcept { return m_status; }

private:
    DensityStatus m_status;
};

/// @}

} // namespace sampling
} // namespace lagrange

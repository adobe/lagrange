/*
 * Copyright 2023 Adobe. All rights reserved.
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

#include <lagrange/testing/api.h>

namespace lagrange::testing {

///
/// Floating point behavior for macOS Xcode on arm64 architectures.
///
enum class FloatPointBehavior {
    // Behavior consistent with Xcode <= 13.
    XcodeLessThan13,

    // Behavior consistent with Xcode >= 14.
    XcodeGreaterThan14,

    // Unknown floating point behavior.
    Unknown,
};

///
/// Detect which Xcode behavior the current program is consistent with.
///
/// Starting from Xcode 14, on arm64 architectures, by default the compiler will
/// implement floating point operations with FMA when possible. This causes a
/// discrepancy with programs compiled on x86_64 platforms, as well as on arm64
/// with previous versions of Xcode (<= 13).
///
/// Note that floating point operations by nature can be very inconsistent
/// between compiler/platforms, and even within the same program. This function
/// should not be relied upon for anything production related, and is only meant
/// to be used for testing purposes.
///
/// @return     Detected float point behavior.
///
LA_TESTING_API FloatPointBehavior detect_fp_behavior();

} // namespace lagrange::testing

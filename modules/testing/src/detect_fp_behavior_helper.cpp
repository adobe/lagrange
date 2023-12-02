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
#include "detect_fp_behavior_helper.h"

namespace lagrange::testing::internal {

FloatPointBehavior detect_fp_behavior_helper(float d00, float d01, float d11)
{
    const float denom = d00 * d11 - d01 * d01;
    const float denom_xcode_le_13 = 0x1.1ac33ep-24;
    const float denom_xcode_ge_14 = 0x1.1ac33cp-24;
    if (denom == denom_xcode_le_13) {
        return FloatPointBehavior::XcodeLessThan13;
    } else if (denom == denom_xcode_ge_14) {
        return FloatPointBehavior::XcodeGreaterThan14;
    } else {
        return FloatPointBehavior::Unknown;
    }
}

} // namespace lagrange::testing::internal

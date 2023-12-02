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
#include <lagrange/testing/detect_fp_behavior.h>

#include "detect_fp_behavior_helper.h"

namespace lagrange::testing {

FloatPointBehavior detect_fp_behavior()
{
    return internal::detect_fp_behavior_helper(0x1.634cd6p-13, -0x1.1b4ec4p-15, 0x1.9e87c6p-12);
}

} // namespace lagrange::testing

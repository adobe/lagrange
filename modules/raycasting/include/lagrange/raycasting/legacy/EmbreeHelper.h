/*
 * Copyright 2017 Adobe. All rights reserved.
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

#include <lagrange/legacy/inline.h>
#include <lagrange/raycasting/api.h>

#ifdef LAGRANGE_WITH_EMBREE_3
    #include <embree3/rtcore.h>
#else
    #include <embree4/rtcore.h>
#endif

RTC_NAMESPACE_USE

namespace lagrange {
namespace raycasting {
LAGRANGE_LEGACY_INLINE
namespace legacy {
namespace EmbreeHelper {

LA_RAYCASTING_API
void ensure_no_errors(const RTCDevice& device);

} // namespace EmbreeHelper
} // namespace legacy
} // namespace raycasting
} // namespace lagrange

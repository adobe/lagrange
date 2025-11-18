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
#ifdef LAGRANGE_WITH_EMBREE_4
    #include <embree4/rtcore.h>
#else
    #include <embree3/rtcore.h>
#endif

#include <lagrange/raycasting/EmbreeHelper.h>
#include <lagrange/utils/assert.h>

#include <cassert>
#include <exception>
#include <sstream>
#include <stdexcept>

RTC_NAMESPACE_USE

void lagrange::raycasting::EmbreeHelper::ensure_no_errors(const RTCDevice& device)
{
    std::stringstream err_msg;
    auto err = rtcGetDeviceError(device);
    switch (err) {
    case RTC_ERROR_NONE: return;
    case RTC_ERROR_UNKNOWN: err_msg << "Embree: unknown error"; break;
    case RTC_ERROR_INVALID_ARGUMENT: err_msg << "Embree: invalid argument"; break;
    case RTC_ERROR_INVALID_OPERATION: err_msg << "Embree: invalid operation"; break;
    case RTC_ERROR_OUT_OF_MEMORY: err_msg << "Embree: out of memory"; break;
    case RTC_ERROR_UNSUPPORTED_CPU: err_msg << "Embree: your CPU does not support SSE2"; break;
    case RTC_ERROR_CANCELLED: err_msg << "Embree: cancelled"; break;
    default: la_runtime_assert(false); err_msg << "Embree: unknown error code: " << err;
    }
    throw std::runtime_error(err_msg.str());
}

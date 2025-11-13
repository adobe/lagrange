/*
 * Copyright 2025 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/internal/cpu_features.h>

#include <lagrange/Logger.h>
#include <lagrange/utils/build.h>

#include <algorithm>
#include <cstdint>
#include <string_view>

// Code adapted from the Tensorflow Standard Libraries:
// https://github.com/google/tsl/blob/fb5b215cc5c3e2ae9ddaaf945566cc0fc652ebf7/tsl/platform/cpu_info.cc
// SPDX-License-Identifier: Apache-2.0

// SIMD extension querying is only available on x86.
#if LAGRANGE_TARGET_PLATFORM(x86_64)
    #if LAGRANGE_TARGET_OS(WINDOWS)
        // Visual Studio defines a builtin function for CPUID, so use that if possible.
        #define GETCPUID(a, b, c, d, a_inp, c_inp) \
            {                                      \
                int cpu_info[4] = {-1};            \
                __cpuidex(cpu_info, a_inp, c_inp); \
                a = cpu_info[0];                   \
                b = cpu_info[1];                   \
                c = cpu_info[2];                   \
                d = cpu_info[3];                   \
            }
    #else
        // Otherwise use gcc-format assembler to implement the underlying instructions.
        #define GETCPUID(a, b, c, d, a_inp, c_inp)   \
            asm("mov %%rbx, %%rdi\n"                 \
                "cpuid\n"                            \
                "xchg %%rdi, %%rbx\n"                \
                : "=a"(a), "=D"(b), "=c"(c), "=d"(d) \
                : "a"(a_inp), "2"(c_inp))
    #endif
#endif

namespace lagrange::internal {

VendorId get_cpu_vendor_id()
{
#if LAGRANGE_TARGET_PLATFORM(arm64)
    return VendorId::ARM;
#elif LAGRANGE_TARGET_PLATFORM(x86_64)
    uint32_t eax, ebx, ecx, edx;

    // Get vendor string (issue CPUID with eax = 0)
    GETCPUID(eax, ebx, ecx, edx, 0, 0);

    char vendor_str[13];
    std::copy_n(reinterpret_cast<char*>(&ebx), 4, vendor_str);
    std::copy_n(reinterpret_cast<char*>(&edx), 4, vendor_str + 4);
    std::copy_n(reinterpret_cast<char*>(&ecx), 4, vendor_str + 8);
    vendor_str[12] = '\0';

    if (std::string_view(vendor_str) == "GenuineIntel") {
        return VendorId::Intel;
    } else if (std::string_view(vendor_str) == "AuthenticAMD") {
        return VendorId::AMD;
    } else {
        logger().debug("Unknown CPU vendor string: {}", vendor_str);
        return VendorId::Unknown;
    }
#else
    return VendorId::Unknown;
#endif
}

} // namespace lagrange::internal

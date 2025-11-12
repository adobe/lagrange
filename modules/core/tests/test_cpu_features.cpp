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

#include <lagrange/Logger.h>
#include <lagrange/internal/cpu_features.h>
#include <lagrange/utils/build.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("cpu features", "[core]")
{
    lagrange::ScopedLogLevel level_guard(spdlog::level::debug);

    auto vendor_id = lagrange::internal::get_cpu_vendor_id();
    switch (vendor_id) {
    case lagrange::internal::VendorId::Intel: lagrange::logger().info("CPU Vendor: Intel"); break;
    case lagrange::internal::VendorId::AMD: lagrange::logger().info("CPU Vendor: AMD"); break;
    case lagrange::internal::VendorId::ARM: lagrange::logger().info("CPU Vendor: ARM"); break;
    case lagrange::internal::VendorId::Unknown:
    default: lagrange::logger().info("CPU Vendor: Unknown"); break;
    }

#if LAGRANGE_TARGET_PLATFORM(x86_64)
    REQUIRE(
        (vendor_id == lagrange::internal::VendorId::Intel ||
         vendor_id == lagrange::internal::VendorId::AMD));
#elif LAGRANGE_TARGET_PLATFORM(arm64)
    REQUIRE(vendor_id == lagrange::internal::VendorId::ARM);
#else
    REQUIRE(vendor_id == lagrange::internal::VendorId::Unknown);
#endif
}

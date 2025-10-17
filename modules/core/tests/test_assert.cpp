/*
 * Copyright 2022 Adobe. All rights reserved.
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
#include <lagrange/testing/common.h>
#include <lagrange/utils/assert.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <catch2/catch_test_macros.hpp>
#include <lagrange/utils/warnon.h>
// clang-format on

TEST_CASE("Assert", "[next]")
{
    la_runtime_assert(true);
    la_runtime_assert(true, "This is true");
    LA_REQUIRE_THROWS(la_runtime_assert(false));
    LA_REQUIRE_THROWS(la_runtime_assert(false, "This is false"));
    LA_REQUIRE_THROWS(la_runtime_assert(false, fmt::format("Complex message: {}", 10)));

    // We want to prevent the macro from taking 3+ arguments:
    // la_runtime_assert(true, "This should not compile", 0);

    la_runtime_assert(true, fmt::format("Hello {}", "world"));

    // The assert macro can be used in an expression:
    int a = 2;
    int b = -1;
    int sum = (la_runtime_assert(a > 0), a) + (la_runtime_assert(b < 0), b);
    REQUIRE(sum == 1);

    bool x = true;
    (void)x;
    la_debug_assert(x);

    la_debug_assert(true);
    la_debug_assert(true, "This is true");
#ifndef NDEBUG
    // Debug mode, should raise exceptions
    lagrange::logger().info("Debug mode");
    LA_REQUIRE_THROWS(la_debug_assert(false));
    LA_REQUIRE_THROWS(la_debug_assert(false, "This is false"));
#else
    // Release mode, no exception should be thrown
    lagrange::logger().info("Release mode");
    REQUIRE_NOTHROW(la_debug_assert(false));
    REQUIRE_NOTHROW(la_debug_assert(false, "This is false"));
#endif // NDEBUG

    int r = 0;
    auto add_to_r = [&]() {
        r++;
        return true;
    };
    (void)add_to_r;
    la_debug_assert(add_to_r());
#ifdef NDEBUG
    REQUIRE(r == 0);
#else
    REQUIRE(r == 1);
#endif

    SUCCEED();
}

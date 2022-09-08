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
#include <lagrange/utils/strings.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <catch2/catch_test_macros.hpp>
#include <lagrange/utils/warnon.h>
// clang-format on

TEST_CASE("String", "[next]")
{
    using namespace lagrange;
    REQUIRE(starts_with("foobar", "foo"));
    REQUIRE(ends_with("foobar", "bar"));
    REQUIRE(starts_with("foobar", ""));
    REQUIRE(ends_with("foobar", ""));
    REQUIRE(starts_with("", ""));
    REQUIRE(ends_with("", ""));

    REQUIRE(!starts_with("foobar", "bar"));
    REQUIRE(!ends_with("foobar", "foo"));
    REQUIRE(!starts_with("", "bar"));
    REQUIRE(!ends_with("", "foo"));
}

/*
 * Copyright 2020 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/testing/common.h>

#include <lagrange/internal/constants.h>
#include <lagrange/utils/utils.h>

TEST_CASE("utils-utils")
{
    using namespace lagrange;

    REQUIRE(to_degrees(0.0) == 0.0);
    REQUIRE(to_degrees(lagrange::internal::pi) == 180.0);

    REQUIRE(to_radians(0.0) == 0.0);
    REQUIRE(to_radians(180.0) == lagrange::internal::pi);

    REQUIRE(sign(20) == 1);
    REQUIRE(sign(-3) == -1);
    REQUIRE(sign(0) == 0);
}

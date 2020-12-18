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

#include <lagrange/ExactPredicates.h>
#include <lagrange/common.h>

TEST_CASE("ExactPredicates", "[ExactPredciates]")
{
    using namespace lagrange;
    auto predicates = ExactPredicates::create("shewchuk");

    SECTION("Simple")
    {
        double a[2]{0.0, 0.0};
        double b[2]{1.0, 0.0};
        double c[2]{0.0, 1.0};

        REQUIRE(predicates->orient2D(a, a, a) == 0);
        REQUIRE(predicates->orient2D(a, a, b) == 0);
        REQUIRE(predicates->orient2D(a, b, c) == 1);
        REQUIRE(predicates->orient2D(a, c, b) == -1);
    }

    SECTION("Debug")
    {
        double p1[2]{23.9314, 23.3408};
        double p2[2]{24.1761, 24.0065};
        double p3[2]{24.4161, 23.8527};
        REQUIRE(predicates->orient2D(p1, p2, p3) != 0);
    }
}

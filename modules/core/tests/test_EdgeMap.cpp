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

#include <limits>

#include <lagrange/Edge.h>
#include <lagrange/common.h>

TEST_CASE("HashSymmetry", "[EdgeMap][Hash][Symmetry]")
{
    using namespace lagrange;
    std::hash<EdgeType<size_t>> hash;
    REQUIRE(hash({{0, 1}}) == hash({{0, 1}}));
    REQUIRE(hash({{1, 0}}) == hash({{0, 1}}));
    REQUIRE(hash({{1, 1}}) != hash({{0, 1}}));

    auto max = std::numeric_limits<size_t>::max();
    auto min = std::numeric_limits<size_t>::min();
    REQUIRE(hash({{max, min}}) == hash({{min, max}}));
    REQUIRE(hash({{max, max}}) == hash({{max, max}}));
    REQUIRE(hash({{min, min}}) == hash({{min, min}}));
    REQUIRE(hash({{min, min}}) != hash({{max, min}}));
}

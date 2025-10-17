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
#include <lagrange/testing/common.h>

#include <lagrange/internal/split_triangle.h>
#include <lagrange/utils/invalid.h>

TEST_CASE("split_triangle", "[core]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SECTION("Case 1")
    {
        // clang-format off
        std::vector<Scalar> points = {
            0.3032455156246821,  -0.24217016299565633, -0.58855189681053166,
            0.30395600030904818, -0.24228679612187093, -0.58940063994237146,
            0.30634476939837141, -0.24267893632253013, -0.59225425720214842,
            0.30642614186670664, -0.24305403257494393, -0.58291655361026795,
            0.30642737150192262, -0.24305970072746277, -0.58277544975280759,
        };
        // clang-format on
        std::vector<Index> chain = {0, 1, 2, 3, 4};
        std::vector<Index> visited_buffer(3 * chain.size(), 0);
        std::vector<Index> queue_buffer;
        queue_buffer.reserve(chain.size());
        std::vector<Index> triangulation(3 * (chain.size() - 2), invalid<Index>());

        internal::split_triangle<Scalar, Index>(
            points.size() / 3,
            lagrange::span<const Scalar>(points),
            lagrange::span<const Index>(chain),
            lagrange::span<Index>(visited_buffer),
            queue_buffer,
            0, // v0
            2, // v1
            4, // v2
            lagrange::span<Index>(triangulation));
        REQUIRE(triangulation[8] != invalid<Index>());
    }
}

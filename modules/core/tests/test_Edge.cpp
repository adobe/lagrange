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

#include <lagrange/Edge.h>
#include <lagrange/common.h>

TEST_CASE("Edge", "[Edge]")
{
    using Edge = lagrange::EdgeType<size_t>;
    Edge e1(0, 1);
    REQUIRE(e1[0] == 0);
    REQUIRE(e1[1] == 1);
    REQUIRE_THROWS(e1[2]);
    REQUIRE(1 == e1.get_other_vertex(0));
    REQUIRE_THROWS(e1.get_other_vertex(12));

    size_t count = 0;
    for (auto v : e1) {
        REQUIRE(e1[count] == v);
        count++;
    }
    REQUIRE(2 == count);

    Edge e2{2, 1};
    REQUIRE(e2 == e2);
    REQUIRE(e1 != e2);
    REQUIRE(e1.has_shared_vertex(e2));
    REQUIRE(1 == e1.get_shared_vertex(e2));
    REQUIRE(1 == e2.get_shared_vertex(e1));

    Edge e3(1, 0);
    REQUIRE(e3 == e1);
    REQUIRE(e3.has_shared_vertex(e1));
    REQUIRE_THROWS(e3.get_shared_vertex(e1));

    Edge e4(2, 3);
    REQUIRE(!e3.has_shared_vertex(e4));
    REQUIRE(e3.get_shared_vertex(e4) == lagrange::INVALID<size_t>());

    Edge e5{10, 10};
    REQUIRE(e5.has_shared_vertex(e5));
    REQUIRE(10 == e5.get_other_vertex(10));

    Edge e_inv = Edge::Invalid();
    REQUIRE(!e_inv.is_valid());
}

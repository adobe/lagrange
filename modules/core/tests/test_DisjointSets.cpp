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

#include <lagrange/DisjointSets.h>

TEST_CASE("DisjointSets", "[disjoint_sets]")
{
    using namespace lagrange;
    DisjointSets<int> data;

    SECTION("Init")
    {
        auto r = data.extract_disjoint_sets();
        REQUIRE(r.size() == 0);

        data.init(10);
        r = data.extract_disjoint_sets();
        REQUIRE(r.size() == 10);

        data.clear();
        r = data.extract_disjoint_sets();
        REQUIRE(r.size() == 0);
    }

    SECTION("Invalid index")
    {
        data.init(10);
        LA_REQUIRE_THROWS(data.find(10));
        LA_REQUIRE_THROWS(data.find(-1));
    }

    SECTION("Cyclic merge")
    {
        data.init(3);
        auto r = data.extract_disjoint_sets();
        REQUIRE(r.size() == 3);

        data.merge(0, 1);
        r = data.extract_disjoint_sets();
        REQUIRE(r.size() == 2);
        REQUIRE(data.find(0) == data.find(1));

        data.merge(1, 2);
        r = data.extract_disjoint_sets();
        REQUIRE(r.size() == 1);
        REQUIRE(data.find(0) == data.find(1));
        REQUIRE(data.find(0) == data.find(2));

        data.merge(2, 0);
        r = data.extract_disjoint_sets();
        REQUIRE(r.size() == 1);
        REQUIRE(data.find(0) == data.find(1));
        REQUIRE(data.find(0) == data.find(2));
    }

    SECTION("Cyclic merge reversed direction")
    {
        data.init(3);
        auto r = data.extract_disjoint_sets();
        REQUIRE(r.size() == 3);

        data.merge(1, 0);
        r = data.extract_disjoint_sets();
        REQUIRE(r.size() == 2);
        REQUIRE(data.find(0) == data.find(1));

        data.merge(2, 1);
        r = data.extract_disjoint_sets();
        REQUIRE(r.size() == 1);
        REQUIRE(data.find(0) == data.find(1));
        REQUIRE(data.find(0) == data.find(2));

        data.merge(0, 2);
        r = data.extract_disjoint_sets();
        REQUIRE(r.size() == 1);
        REQUIRE(data.find(0) == data.find(1));
        REQUIRE(data.find(0) == data.find(2));
    }
}

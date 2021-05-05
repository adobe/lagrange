/*
 * Copyright 2021 Adobe. All rights reserved.
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

#include <lagrange/utils/SmallSet.h>

TEST_CASE("SmallSet", "[utils]")
{
    // Create initial set
    lagrange::SmallSet<int, 5> set = {1, 2, 4, 4, 0};
    REQUIRE(set.contains(0));
    REQUIRE(set.contains(1));
    REQUIRE(set.contains(2));
    REQUIRE(set.contains(4));
    REQUIRE(set.size() == 4);

    // Insertion of existing key
    REQUIRE(set.insert(2).second == false);
    REQUIRE(set.contains(0));
    REQUIRE(set.contains(1));
    REQUIRE(set.contains(2));
    REQUIRE(set.contains(4));
    REQUIRE(set.size() == 4);

    // Erase non-existing key
    REQUIRE(set.erase(6) == 0);
    REQUIRE(set.contains(0));
    REQUIRE(set.contains(1));
    REQUIRE(set.contains(2));
    REQUIRE(set.contains(4));
    REQUIRE(set.size() == 4);

    // Erase existing key
    REQUIRE(set.erase(2) == 1);
    REQUIRE(set.contains(0));
    REQUIRE(set.contains(1));
    REQUIRE(!set.contains(2));
    REQUIRE(set.contains(4));
    REQUIRE(set.size() == 3);

    // Add new key
    REQUIRE(set.insert(9).second == true);
    REQUIRE(set.contains(0));
    REQUIRE(set.contains(1));
    REQUIRE(!set.contains(2));
    REQUIRE(set.contains(4));
    REQUIRE(set.contains(9));
    REQUIRE(set.size() == 4);
}

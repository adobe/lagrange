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

#include <lagrange/utils/strings.h>

TEST_CASE("utils-strings")
{
    using namespace lagrange;

    std::string s1 = "Hello World!";
    auto split1 = string_split(s1, ' ');
    REQUIRE(split1.size() == 2);
    REQUIRE(split1[0] == "Hello");
    REQUIRE(split1[1] == "World!");

    REQUIRE(string_split("The quick brown fox jumps over the lazy dog", ' ').size() == 9);


    REQUIRE(lagrange::string_format("Hello {}", "World!") == "Hello World!");
    REQUIRE(lagrange::string_format("{} == {}", 0, 1) == "0 == 1");
    REQUIRE(lagrange::string_format("{} == {}", "0", 1) == "0 == 1");
}

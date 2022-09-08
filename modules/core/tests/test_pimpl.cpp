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
#include <lagrange/utils/value_ptr.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <catch2/catch_test_macros.hpp>
#include <lagrange/utils/warnon.h>
// clang-format on

TEST_CASE("Pimpl: Const Propagation", "[next]")
{
    struct Obj
    {
        lagrange::value_ptr<int> pimpl_ptr = lagrange::value_ptr<int>(10);
        std::shared_ptr<int> shared_ptr = std::make_shared<int>(10);
    };
    Obj x;
    REQUIRE(x.pimpl_ptr);
    REQUIRE(*x.pimpl_ptr == 10);
    *x.pimpl_ptr = 20;
    *x.shared_ptr = 20;

    const Obj y = x;
    // *y.pimpl_ptr = 30; // <-- this will not compile (which is good!)
    *y.shared_ptr = 30;

    REQUIRE(y.pimpl_ptr.get() != x.pimpl_ptr.get());
    REQUIRE(y.shared_ptr.get() == x.shared_ptr.get());
}

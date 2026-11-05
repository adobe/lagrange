/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/experimental/Array.h>
#include <lagrange/utils/build.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>

// Exercise the same RTTI checks natively and across Emscripten main/side modules.
#if defined(LAGRANGE_TEST_CROSS_DSO_HELPER) && LAGRANGE_ENABLE_RTTI

    #include <dlfcn.h>

TEST_CASE("experimental::ArrayBase::down_cast across DSO boundary", "[experimental][down_cast]")
{
    using namespace lagrange::experimental;
    using ArrayImpl = EigenArray<Eigen::MatrixXd>;
    using OtherArrayImpl = EigenArray<Eigen::MatrixXf>;

    std::unique_ptr<void, int (*)(void*)> handle(
        dlopen(LAGRANGE_TEST_CROSS_DSO_HELPER_PATH, RTLD_NOW | RTLD_LOCAL),
        &dlclose);
    REQUIRE(handle != nullptr);

    using CreateFn = ArrayBase* (*)();
    using DestroyFn = void (*)(ArrayBase*);

    auto create =
        reinterpret_cast<CreateFn>(dlsym(handle.get(), "lagrange_test_create_cross_dso_array"));
    auto destroy =
        reinterpret_cast<DestroyFn>(dlsym(handle.get(), "lagrange_test_destroy_cross_dso_array"));
    REQUIRE(create != nullptr);
    REQUIRE(destroy != nullptr);

    std::unique_ptr<ArrayBase, DestroyFn> array(create(), destroy);
    REQUIRE(array != nullptr);
    ArrayBase* base = array.get();

    auto* cast = dynamic_cast<ArrayImpl*>(base);
    REQUIRE(cast != nullptr);
    auto* derived = base->down_cast<ArrayImpl*>();
    REQUIRE(derived != nullptr);
    CHECK(derived == cast);
    CHECK(derived->get_ref()(0, 0) == 1);
    CHECK(derived->get_ref()(1, 1) == 4);

    const ArrayBase* const_base = base;
    const auto* const_cast_result = dynamic_cast<const ArrayImpl*>(const_base);
    REQUIRE(const_cast_result != nullptr);
    const auto* const_derived = const_base->down_cast<const ArrayImpl*>();
    REQUIRE(const_derived != nullptr);
    CHECK(const_derived == const_cast_result);
    CHECK(const_derived->get_ref()(1, 0) == 3);

    CHECK(dynamic_cast<OtherArrayImpl*>(base) == nullptr);
    CHECK(base->down_cast<OtherArrayImpl*>() == nullptr);
    CHECK(dynamic_cast<const OtherArrayImpl*>(const_base) == nullptr);
    CHECK(const_base->down_cast<const OtherArrayImpl*>() == nullptr);
}

#endif

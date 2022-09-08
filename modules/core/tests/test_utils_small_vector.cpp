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
#include <lagrange/testing/common.h>

#include <lagrange/utils/SmallVector.h>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/benchmark/catch_constructor.hpp>

namespace {
constexpr size_t SMALL_SIZE = 4;
constexpr size_t LARGE_SIZE = 400000;
} // namespace

TEST_CASE("utils-SmallVector", "[utils][SmallVector]")
{
    using namespace lagrange;

    SmallVector<int, SMALL_SIZE> v = {0, 1, 2, 3};
    REQUIRE(v.size() == SMALL_SIZE);
    for (size_t i = 0; i < SMALL_SIZE; ++i) REQUIRE(v[i] == (int)i);

    v.push_back(4);
    REQUIRE(v.size() == SMALL_SIZE + 1);
    for (size_t i = 0; i < SMALL_SIZE + 1; ++i) REQUIRE(v[i] == (int)i);

    SmallVector<int, SMALL_SIZE> w;
    REQUIRE(w.empty());
    std::swap(v, w);
    REQUIRE(v.empty());
    REQUIRE(w.size() == SMALL_SIZE + 1);
}

TEST_CASE("utils-SmallVector-benchmark", "[utils][SmallVector][!benchmark]")
{
    using namespace lagrange;

    BENCHMARK_ADVANCED("utils-SmallVector-small-construct")
    (Catch::Benchmark::Chronometer meter)
    {
        std::vector<Catch::Benchmark::storage_for<SmallVector<int, SMALL_SIZE>>> storage(
            meter.runs());
        meter.measure([&](size_t i) { storage[i].construct(); });
    };

    BENCHMARK_ADVANCED("utils-SmallVector-std-construct")
    (Catch::Benchmark::Chronometer meter)
    {
        std::vector<Catch::Benchmark::storage_for<std::vector<int>>> storage(meter.runs());
        meter.measure([&](size_t i) { storage[i].construct(); });
    };

    BENCHMARK("utils-SmallVector-small-push_back-small")
    {
        SmallVector<int, SMALL_SIZE> v;
        for (size_t i = 0; i < SMALL_SIZE; ++i) v.push_back((int)i);
        return v.size();
    };

    BENCHMARK("utils-SmallVector-std-push_back-small")
    {
        std::vector<int> v;
        for (size_t i = 0; i < SMALL_SIZE; ++i) v.push_back((int)i);
        return v.size();
    };

    BENCHMARK("utils-SmallVector-small-push_back-large")
    {
        SmallVector<int, SMALL_SIZE> v;
        for (size_t i = 0; i < LARGE_SIZE; ++i) v.push_back((int)i);
        return v.size();
    };

    BENCHMARK("utils-SmallVector-std-push_back-large")
    {
        std::vector<int> v;
        for (size_t i = 0; i < LARGE_SIZE; ++i) v.push_back((int)i);
        return v.size();
    };

    {
        SmallVector<int, SMALL_SIZE> v;
        v.resize(LARGE_SIZE);
        BENCHMARK("utils-SmallVector-small-clear_and_push_back")
        {
            v.clear();
            for (size_t i = 0; i < LARGE_SIZE; ++i) v.push_back((int)i);
            return v.size();
        };
    }

    {
        std::vector<int> v;
        v.resize(LARGE_SIZE);
        BENCHMARK("utils-SmallVector-std-clear_and_push_back")
        {
            v.clear();
            for (size_t i = 0; i < LARGE_SIZE; ++i) v.push_back((int)i);
            return v.size();
        };
    }
}

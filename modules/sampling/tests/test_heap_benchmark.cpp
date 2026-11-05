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
#include <lagrange/common.h>
#include <lagrange/sampling/internal/HeapWithHash.h>
#include <lagrange/sampling/internal/HeapWithVector.h>

#include <lagrange/testing/common.h>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <iostream>
#include <random>

template <typename Heap>
void run_heap_benchmark(
    const std::string& prefix,
    const typename Heap::Pair::first_type number_of_key,
    const size_t number_of_pair)
{
    using Pair = typename Heap::Pair;
    using Key = typename Pair::first_type;
    using Value = typename Pair::second_type;

    using Input = std::vector<Pair>;
    using Truth = std::unordered_map<Key, Value>;
    using Data = std::tuple<Input, Truth>;

    std::mt19937_64 rng(0x4578af41);
    std::uniform_int_distribution<Key> dist_key(0, number_of_key);
    std::normal_distribution<Value> dist_weight(0, 1);
    const auto generate_data = [&]() -> Data {
        Truth truth;
        Input input;
        input.reserve(number_of_pair);
        for (size_t kk = 0; kk < number_of_pair; kk++) {
            const auto key = dist_key(rng);
            const auto weight = dist_weight(rng);
            const Pair pair{key, weight};
            input.emplace_back(pair);

            auto iter = truth.find(key);
            if (iter != std::cend(truth))
                iter->second = std::min(iter->second, weight);
            else
                truth.emplace(pair);
        }
        return {std::move(input), std::move(truth)};
    };

    { // manual construction then cleanup
        std::stringstream ss;
        ss << prefix << " manual construction then cleanup nkey=" << number_of_key
           << " npair=" << number_of_pair;
        BENCHMARK_ADVANCED(ss.str())(Catch::Benchmark::Chronometer meter)
        {
            const auto data = generate_data();

            Heap heap;
            meter.measure([&]() {
                heap = Heap();
                for (const auto& pair : std::get<0>(data))
                    heap.insert_or_update(pair.first, pair.second);
                heap.cleanup(0);
                return heap;
            });

            REQUIRE(heap.size() == std::get<1>(data).size());
            auto prev_weight = std::numeric_limits<Value>::infinity();
            while (!heap.empty()) {
                const auto top = heap.pop();

                const auto truth_iter = std::get<1>(data).find(top.first);
                REQUIRE(truth_iter != std::cend(std::get<1>(data)));
                REQUIRE(truth_iter->first == top.first);
                REQUIRE(truth_iter->second == top.second);

                REQUIRE(prev_weight >= top.second);
                prev_weight = top.second;
            }
        };
    }

    { // range construction then cleanup
        std::stringstream ss;
        ss << prefix << " range construction then cleanup nkey=" << number_of_key
           << " npair=" << number_of_pair;
        BENCHMARK_ADVANCED(ss.str())(Catch::Benchmark::Chronometer meter)
        {
            const auto data = generate_data();

            Heap heap;
            meter.measure([&]() {
                heap = Heap(std::cbegin(std::get<0>(data)), std::cend(std::get<0>(data)));
                heap.cleanup(0);
                return heap;
            });

            REQUIRE(heap.size() == std::get<1>(data).size());
            auto prev_weight = std::numeric_limits<Value>::infinity();
            while (!heap.empty()) {
                const auto top = heap.pop();

                const auto truth_iter = std::get<1>(data).find(top.first);
                REQUIRE(truth_iter != std::cend(std::get<1>(data)));
                REQUIRE(truth_iter->first == top.first);
                REQUIRE(truth_iter->second == top.second);

                REQUIRE(prev_weight >= top.second);
                prev_weight = top.second;
            }
        };
    }

    { // popping data
        std::stringstream ss;
        ss << prefix << " popping data nkey=" << number_of_key << " npair=" << number_of_pair;
        BENCHMARK_ADVANCED(ss.str())(Catch::Benchmark::Chronometer meter)
        {
            const auto data = generate_data();

            Heap heap;
            heap = Heap(std::cbegin(std::get<0>(data)), std::cend(std::get<0>(data)));
            heap.cleanup(0);

            Value accum_weight_ = 0;
            meter.measure([&]() {
                assert(heap.size() == std::get<1>(data).size());
                auto heap_ = heap;
                accum_weight_ = 0;
                while (!heap_.empty()) {
                    const auto top = heap_.pop();
                    accum_weight_ += top.second;
                }
                return accum_weight_;
            });

            Value accum_weight = 0;
            for (const auto& pair : std::get<1>(data)) accum_weight += pair.second;

            const auto error = std::abs(accum_weight - accum_weight_);
            REQUIRE(error < 1e-7);
        };
    }
}

TEST_CASE("Heap benchmark", "[sampling][!benchmark]")
{
    for (unsigned int kk = 64; kk < 1024; kk *= 2) {
        run_heap_benchmark<lagrange::sampling::internal::HeapWithHash<unsigned int, double>>(
            "HH",
            kk,
            4 * kk);
        run_heap_benchmark<lagrange::sampling::internal::HeapWithVector<unsigned int, double>>(
            "VV",
            kk,
            4 * kk);
    }
}

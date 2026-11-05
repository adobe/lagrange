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

#include <lagrange/testing/common.h>

#include <chrono>
#include <random>

template <typename Heap>
void pop_and_check(Heap& heap, const std::vector<typename Heap::Pair>& truth)
{
    auto truth_iter = std::cbegin(truth);
    REQUIRE(heap.size() == truth.size());

    while (!heap.empty()) {
        const auto aa = heap.pop();

        REQUIRE(truth_iter != std::cend(truth));
        REQUIRE(aa == *truth_iter);

        truth_iter++;
    }

    REQUIRE(heap.empty());
}

TEST_CASE("Hash heap insertion", "[sampling]")
{
    using Heap = lagrange::sampling::internal::HeapWithHash<std::string, int>;

    Heap heap;

    REQUIRE(heap.empty());
    REQUIRE(heap.size() == 0);
    REQUIRE(heap.heap_size() == 0);

    {
        const bool insert_was_minimum = heap.insert_or_update("coucou", 1);

        REQUIRE(!heap.empty());
        REQUIRE(heap.size() == 1);
        REQUIRE(heap.heap_size() == 1);
        REQUIRE(insert_was_minimum);
    }

    {
        const bool insert_was_minimum = heap.insert_or_update("hello", 2);

        REQUIRE(!heap.empty());
        REQUIRE(heap.size() == 2);
        REQUIRE(heap.heap_size() == 2);
        REQUIRE(insert_was_minimum);
    }

    {
        const bool insert_was_minimum = heap.insert_or_update("world", 0);

        REQUIRE(!heap.empty());
        REQUIRE(heap.size() == 3);
        REQUIRE(heap.heap_size() == 3);
        REQUIRE(insert_was_minimum);
    }

    {
        const bool insert_was_minimum = heap.insert_or_update("hello", 2);

        REQUIRE(!heap.empty());
        REQUIRE(heap.size() == 3);
        REQUIRE(heap.heap_size() == 3);
        REQUIRE(!insert_was_minimum);
    }

    {
        const bool insert_was_minimum = heap.insert_or_update("coucou", -1);

        REQUIRE(!heap.empty());
        REQUIRE(heap.size() == 3);
        REQUIRE(heap.heap_size() == 4);
        REQUIRE(insert_was_minimum);
    }

    {
        const bool insert_was_minimum = heap.insert_or_update("coucou", 10);

        REQUIRE(!heap.empty());
        REQUIRE(heap.size() == 3);
        REQUIRE(heap.heap_size() == 4);
        REQUIRE(!insert_was_minimum);
    }

    {
        const bool insert_was_minimum = heap.insert_or_update("world", 10);

        REQUIRE(!heap.empty());
        REQUIRE(heap.size() == 3);
        REQUIRE(heap.heap_size() == 4);
        REQUIRE(!insert_was_minimum);
    }

    pop_and_check(
        heap,
        {
            {"hello", 2},
            {"world", 0},
            {"coucou", -1},
        });
}

TEST_CASE("Hash heap cleanup", "[sampling]")
{
    using Heap = lagrange::sampling::internal::HeapWithHash<std::string, int>;

    Heap heap;

    REQUIRE(heap.empty());
    REQUIRE(heap.size() == 0);

    for (int kk = 8; kk >= 0; kk--) {
        const bool insert_was_minimum = heap.insert_or_update("coucou", kk);

        REQUIRE(!heap.empty());
        REQUIRE(heap.size() == 1);
        REQUIRE(heap.heap_size() + kk == 9);
        REQUIRE(insert_was_minimum);
    }

    {
        const bool insert_was_minimum = heap.insert_or_update("hello", 5);

        REQUIRE(!heap.empty());
        REQUIRE(heap.size() == 2);
        REQUIRE(heap.heap_size() == 10);
        REQUIRE(insert_was_minimum);
    }

    SECTION("Factor too large to trigger cleanup")
    {
        const size_t cleaned_count = heap.cleanup(6);

        REQUIRE(!heap.empty());
        REQUIRE(heap.size() == 2);
        REQUIRE(heap.heap_size() == 10);
        REQUIRE(cleaned_count == 0);
    }

    SECTION("Trigger cleanup")
    {
        const size_t cleaned_count = heap.cleanup(5);

        REQUIRE(!heap.empty());
        REQUIRE(heap.size() == 2);
        REQUIRE(heap.heap_size() == 2);
        REQUIRE(cleaned_count == 8);
    }

    SECTION("Trigger cleanup")
    {
        const size_t cleaned_count = heap.cleanup(4);

        REQUIRE(!heap.empty());
        REQUIRE(heap.size() == 2);
        REQUIRE(heap.heap_size() == 2);
        REQUIRE(cleaned_count == 8);
    }

    {
        Heap heap_ = heap;
        pop_and_check(
            heap_,
            {
                {"hello", 5},
                {"coucou", 0},
            });
    }

    { // force cleanup
        heap.cleanup(0);

        REQUIRE(!heap.empty());
        REQUIRE(heap.size() == 2);
        REQUIRE(heap.heap_size() == 2);
    }

    pop_and_check(
        heap,
        {
            {"hello", 5},
            {"coucou", 0},
        });
}

TEST_CASE("Hash heap range construction", "[sampling]")
{
    using Heap = lagrange::sampling::internal::HeapWithHash<size_t, double>;
    using Pairs = std::vector<Heap::Pair>;

    const Pairs pairs{
        {1, -100},
        {0, 100},
    };

    Heap heap(std::cbegin(pairs), std::cend(pairs));

    pop_and_check(
        heap,
        {
            {0, 100},
            {1, -100},
        });
}

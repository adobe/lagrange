/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
////////////////////////////////////////////////////////////////////////////////
#include "../src/ThreadPool.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("ThreadPool", "[filtering][thread]")
{
    std::atomic_bool is_thread_index_valid = true;
    std::atomic_bool is_loop_index_valid = true;
    unsigned int num_threads = lagrange::filtering::threadpool::ThreadPool::NumThreads();
    REQUIRE(num_threads > 0);
    lagrange::filtering::threadpool::ThreadPool::ParallelFor(0, 1000, [&](int tid, size_t i) {
        unsigned int thread_index = static_cast<unsigned int>(tid);
        if (tid < 0 || thread_index >= num_threads) {
            is_thread_index_valid = false;
        }
        if (i >= 1000) {
            is_loop_index_valid = false;
        }
    });
    REQUIRE(is_thread_index_valid);
    REQUIRE(is_loop_index_valid);
}

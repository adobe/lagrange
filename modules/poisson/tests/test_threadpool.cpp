////////////////////////////////////////////////////////////////////////////////
#include "../src/ThreadPool.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("ThreadPool", "[poisson][thread]")
{
    std::atomic_bool is_thread_index_valid = true;
    std::atomic_bool is_loop_index_valid = true;
    unsigned int num_threads = lagrange::poisson::threadpool::ThreadPool::NumThreads();
    REQUIRE(num_threads > 0);
    lagrange::poisson::threadpool::ThreadPool::ParallelFor(0, 1000, [&](int tid, size_t i) {
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

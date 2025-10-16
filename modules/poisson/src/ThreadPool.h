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

#pragma once

#include <lagrange/utils/assert.h>

#include <tbb/parallel_for.h>
#include <tbb/parallel_invoke.h>
#include <tbb/task_arena.h>

namespace lagrange::poisson::threadpool {

struct ThreadPool
{
    // Used to create thread-local data for map-reduce operations
    static unsigned int NumThreads(void) { return tbb::this_task_arena::max_concurrency(); }

    // Execute multiple functions in parallel
    template <typename... Functions>
    static void ParallelSections(Functions&&... funcs)
    {
        tbb::parallel_invoke(std::forward<Functions>(funcs)...);
    }

    // Execute a function in parallel over a range of indices
    template <typename Function>
    static void ParallelFor(size_t begin, size_t end, Function&& func)
    {
        // Keeping this commented block for quick debugging of multithread issues.
#if 0
        int thread_index = 0;
        for (size_t i = begin; i < end; ++i) {
            func(thread_index, i);
        }
#else
        tbb::parallel_for(
            tbb::blocked_range<size_t>(begin, end),
            [&](const tbb::blocked_range<size_t>& r) {
                int thread_index = tbb::this_task_arena::current_thread_index();
                la_debug_assert(thread_index != tbb::task_arena::not_initialized);
                for (size_t i = r.begin(); i < r.end(); ++i) {
                    func(thread_index, i);
                }
            });

#endif
    }
};

} // namespace lagrange::poisson::threadpool

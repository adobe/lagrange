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
#include <lagrange/utils/tbb.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/task.h>
#include <tbb/task_group.h>
#include <tbb/blocked_range.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#ifndef TBB_INTERFACE_VERSION_MAJOR
    #error "TBB_INTERFACE_VERSION_MAJOR macro is not defined"
#endif

namespace lagrange {

namespace tbb_utils {

bool cancel_group_execution()
{
#if TBB_INTERFACE_VERSION_MAJOR >= 12
    return tbb::task::current_context()->cancel_group_execution();
#else
    return tbb::task::self().cancel_group_execution();
#endif
}

bool is_cancelled()
{
#if TBB_INTERFACE_VERSION_MAJOR >= 12
    return tbb::task::current_context()->is_group_execution_cancelled();
#else
    return tbb::task::self().is_cancelled();
#endif
}

} // namespace tbb_utils

} // namespace lagrange

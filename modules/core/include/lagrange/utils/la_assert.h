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
#pragma once

#include <sstream>
#include <cassert>

/*
This file defines lagrange's LA_ASSERT macro and related functionality
*/

namespace lagrange {

template <typename T>
void throw_LA_ASSERT(const T msg, const char* file, int line)
{
    std::ostringstream oss;
    oss << "Lagrange Error: " << file << " line: " << line << " " << msg;
    throw std::runtime_error(oss.str());
}

#define LA_GET_3RD_ARG_HELPER(arg1, arg2, arg3, ...) arg3

#define LA_Assert_1(cond)                                  \
    if (!(cond)) {                                         \
        lagrange::throw_LA_ASSERT("", __FILE__, __LINE__); \
    }

#define LA_Assert_2(cond, msg)                              \
    if (!(cond)) {                                          \
        lagrange::throw_LA_ASSERT(msg, __FILE__, __LINE__); \
    }

#ifdef _WIN32 // special VS variadics

#define LA_MSVS_EXPAND(x) x

#define LA_ASSERT(...)                                                           \
    LA_MSVS_EXPAND(LA_GET_3RD_ARG_HELPER(__VA_ARGS__, LA_Assert_2, LA_Assert_1)) \
    LA_MSVS_EXPAND((__VA_ARGS__))

#else
#define LA_ASSERT(...) LA_GET_3RD_ARG_HELPER(__VA_ARGS__, LA_Assert_2, LA_Assert_1)(__VA_ARGS__)
#endif

#ifndef LA_ASSERT_DEBUG
#if !defined(NDEBUG)
#define LA_ASSERT_DEBUG(x) do { assert(x); } while(0)
#else
#define LA_ASSERT_DEBUG(x) (void)(x)
#endif
#endif

} // namespace lagrange

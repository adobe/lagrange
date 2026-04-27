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
#pragma once

///
/// @file fmt/format.h
///
/// Provides `lagrange::format`, `lagrange::format_string`, and `lagrange::ptr` — uniform wrappers
/// that dispatch to either `std::format` or `{fmt}` depending on `SPDLOG_USE_STD_FORMAT`.
///

#ifdef SPDLOG_USE_STD_FORMAT

    #include <format>

namespace lagrange {

template <typename... Args>
using format_string = std::format_string<Args...>;

using std::format;

/// Equivalent to `fmt::ptr`. Formats a pointer for output.
template <typename T>
const void* ptr(const T* p)
{
    return static_cast<const void*>(p);
}

} // namespace lagrange

#else

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <spdlog/fmt/fmt.h>
#include <lagrange/utils/warnon.h>
// clang-format on


namespace lagrange {

template <typename... Args>
using format_string = fmt::format_string<Args...>;

using fmt::format;
using fmt::ptr;

} // namespace lagrange

#endif

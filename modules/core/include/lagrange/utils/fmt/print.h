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
/// @file fmt/print.h
///
/// Provides `lagrange::print` — writes a formatted string to an output stream.
///

#include <lagrange/utils/fmt/format.h>

#if !defined(SPDLOG_USE_STD_FORMAT) && FMT_VERSION >= 90000
// clang-format off
#include <lagrange/utils/warnoff.h>
#include <spdlog/fmt/ostr.h>
#include <lagrange/utils/warnon.h>
// clang-format on
#endif

#include <ostream>

namespace lagrange {

/// Writes a formatted string to an output stream.
template <typename... Args>
void print(std::ostream& os, format_string<Args...> fmt_str, Args&&... args)
{
#if defined(SPDLOG_USE_STD_FORMAT)
    // std::print is only available since C++23
    os << std::format(fmt_str, std::forward<Args>(args)...);
#elif FMT_VERSION >= 90000
    // fmt v9+ accepts format_string<T...> directly in fmt::print(ostream, ...).
    fmt::print(os, fmt_str, std::forward<Args>(args)...);
#else
    // fmt v8 uses `to_string_view(const S&)` internally, which doesn't recognize
    // basic_format_string as a string-like type. Fall back to format + ostream write.
    os << fmt::format(fmt_str, std::forward<Args>(args)...);
#endif
}

} // namespace lagrange

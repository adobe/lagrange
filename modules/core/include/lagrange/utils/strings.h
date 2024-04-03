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

#include <lagrange/api.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <spdlog/fmt/fmt.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <string>
#include <string_view>
#include <utility>
#include <vector>


namespace lagrange {

/// @defgroup group-utils-misc Miscellaneous
/// @ingroup group-utils
/// Useful functions that don't have their place anywhere else.
/// @{

///
/// Split a std::string using a prescribed delimiter.
///
/// @param[in]  str        String to split.
/// @param[in]  delimiter  Delimiter.
///
/// @return     An array of strings obtained after splitting.
///
LA_CORE_API std::vector<std::string> string_split(const std::string& str, char delimiter);

///
/// Checks if the string begins with the given prefix.
///
/// @param[in]  str     The string.
/// @param[in]  prefix  The prefix.
///
/// @note       Can be replaced with the standard library in C++20
///
/// @return     true if the string begins with the provided prefix, false otherwise.
///
LA_CORE_API bool starts_with(std::string_view str, std::string_view prefix);

///
/// Checks if the string ends with the given suffix.
///
/// @param[in]  str     The string.
/// @param[in]  suffix  The suffix.
///
/// @note       Can be replaced with the standard library in C++20
///
/// @return     true if the string end with the provided suffix, false otherwise.
///
LA_CORE_API bool ends_with(std::string_view str, std::string_view suffix);

///
/// Convert a string to lowercase.
///
/// @param[in]  str     The input string.
///
/// @return     The same string converted to lowercase.
///
/// @note This method assumes the input string is ASCII.
///
LA_CORE_API std::string to_lower(std::string str);

///
/// Convert a string to uppercase.
///
/// @param[in]  str     The input string.
///
/// @return     The same string converted to uppercase.
///
/// @note This method assumes the input string is ASCII.
///
LA_CORE_API std::string to_upper(std::string str);

///
/// Format args according to the format string fmt, and return the result as a string.
///
/// @note       Can be replaced with std::format in C++20
///
/// @param[in]  format  An object that represents the format string.
/// @param[in]  args    Arguments to be formatted.
///
/// @tparam     Args    Types of the arguments to be formatted.
///
/// @return     A string object holding the formatted result.
///
template <typename... Args>
std::string string_format(fmt::format_string<Args...> format, Args&&... args)
{
    return fmt::format(format, std::forward<Args>(args)...);
}

/// @}

} // namespace lagrange

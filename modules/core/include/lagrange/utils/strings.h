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
#include <string>
#include <utility>
#include <vector>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <spdlog/fmt/fmt.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange {

std::vector<std::string> string_split(const std::string& s, char delimiter);

// Can be replaced with the standard library in C++20
bool ends_with(const std::string& str, const std::string& suffix);

bool starts_with(const std::string& str, const std::string& prefix);

// Can be replaced with std::format in C++20
template <typename... Args>
std::string string_format(const std::string& format, Args&&... args)
{
    return fmt::format(format, std::forward<Args>(args)...);
}

} // namespace lagrange

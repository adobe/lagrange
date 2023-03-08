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
#include <lagrange/utils/strings.h>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace lagrange {

std::vector<std::string> string_split(const std::string& s, char delimiter)
{
    std::istringstream iss(s);
    std::vector<std::string> words;
    std::string word;
    while (std::getline(iss, word, delimiter)) {
        words.push_back(word);
    }
    return words;
}

bool ends_with(std::string_view str, std::string_view suffix)
{
    if (str.length() < suffix.length()) return false;
    return (0 == str.compare(str.length() - suffix.length(), suffix.length(), suffix));
}


bool starts_with(std::string_view str, std::string_view prefix)
{
    return (str.rfind(prefix, 0) == 0);
}

std::string to_lower(std::string str)
{
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return str;
}

std::string to_upper(std::string str)
{
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return std::toupper(c);
    });
    return str;
}

} // namespace lagrange

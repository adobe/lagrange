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

bool ends_with(const std::string& str, const std::string& suffix)
{
    if (str.length() < suffix.length()) return false;
    return (0 == str.compare(str.length() - suffix.length(), suffix.length(), suffix));
}

bool starts_with(const std::string& str, const std::string& prefix)
{
    return (str.rfind(prefix, 0) == 0);
}
} // namespace lagrange

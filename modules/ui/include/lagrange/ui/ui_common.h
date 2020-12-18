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

#include <string>
#include <lagrange/experimental/Scalar.h>

#ifdef __GNUC__
#define LGUI_UNUSED(x) LGUI_UNUSED_##x __attribute__((__unused__))
#else
#define LGUI_UNUSED(x) LGUI_UNUSED_##x
#endif

namespace lagrange {
namespace ui {

namespace utils {

template <typename T>
struct type_string {
    static std::string value() { return lagrange::experimental::ScalarToEnum<T>::name; }
};


struct pair_hash {
    template <typename First, typename Second>
    std::size_t operator()(const std::pair<First, Second> & key) const
    {
        return std::hash<decltype(key.first)>()(key.first) ^
               std::hash<decltype(key.second)>()(key.second);
    }
};

}

}
}

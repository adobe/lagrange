/*
 * Copyright 2022 Adobe. All rights reserved.
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

#include <type_traits>
#include <limits>

namespace lagrange {

/// @defgroup group-utils-misc Miscellaneous
/// @ingroup group-utils
/// Useful functions that don't have their place anywhere else.
/// @{

///
/// You can use invalid<T>() to get a value that can represent "invalid" values, such as indices.
/// invalid<T>() is guaranteed to always be the same value for a given type T.
///
/// This is supported for arithmetic types, and returns `numeric_limits<T>::max()`.
///
/// @tparam     T     Index type.
///
/// @return     A value considered invalid for the given type.
///
template <typename T>
constexpr T invalid()
{
    static_assert(!std::is_same<T, bool>::value, "Do not use invalid<bool>() !");
    static_assert(
        std::is_arithmetic<T>::value,
        "invalid<T> is only supported for arithmetic types");
    return std::numeric_limits<T>::max();
}

/// @}

} // namespace lagrange

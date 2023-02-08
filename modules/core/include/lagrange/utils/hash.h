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

#include <algorithm>
#include <functional>
#include <type_traits>
#include <utility>

namespace lagrange {

/// @addtogroup group-utils-misc
/// @{

/// Hash an object @a v and combine it with an existing hash value @a seed. <b>NOT commutative.</b>
///
/// Copied from https://www.boost.org/doc/libs/1_64_0/boost/functional/hash/hash.hpp.
/// SPDX-License-Identifier: BSL-1.0
template <class T>
void hash_combine(size_t& seed, const T& v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) +
            (seed >> 2); // magic random number ensures spreading of hashes
}

namespace detail {

template <typename U, typename V>
size_t ordered_pair_hash_value(const U& u, const V& v)
{
    size_t h = std::hash<U>{}(u);
    hash_combine(h, v);
    return h;
}

} // namespace detail

/// Compute an order-dependent hash of a pair of values. The default template handles `std::array`,
/// `std::vector`, Eigen vectors, raw pointers/C-arrays, and other classes with an array indexing
/// `operator[]`. (Note that only the first two elements of the array will be hashed.) Other
/// specializations handle `std::pair` etc.
template <typename T, typename Enable = void>
struct OrderedPairHash
{
    size_t operator()(const T& k) const { return detail::ordered_pair_hash_value(k[0], k[1]); }
};

template <typename U, typename V>
struct OrderedPairHash<std::pair<U, V>>
{
    size_t operator()(const std::pair<U, V>& k) const
    {
        return detail::ordered_pair_hash_value(k.first, k.second);
    }
};

/// @}

} // namespace lagrange

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
#include <lagrange/testing/common.h>

#include <lagrange/Logger.h>
#include <lagrange/utils/hash.h>
#include <unordered_set>

using namespace lagrange;

namespace {

template <typename Key, typename U, typename V>
void init_key(Key& k, const U& u, const V& v)
{
    k = Key(u, v);
}

template <typename T>
void init_key(T*& k, const T& u, const T& v)
{
    k = new T[2];
    k[0] = u;
    k[1] = v;
}

template <typename T>
void init_key(std::array<T, 2>& k, const T& u, const T& v)
{
    k[0] = u;
    k[1] = v;
}

template <typename Key>
void destroy_key(Key&)
{}

template <typename T>
void destroy_key(T* k)
{
    delete[] k;
}

} // namespace

template <typename Key>
void test_ordered()
{
    Key k0, k1;
    init_key(k0, 0, 1);
    init_key(k1, 1, 0);
    OrderedPairHash<Key> hasher;
    auto h0 = hasher(k0);
    auto h1 = hasher(k1);
    REQUIRE(h0 != h1); // hash collisions should be vanishingly improbable

    std::unordered_set<Key, OrderedPairHash<Key>> s;
    s.insert(k0);
    s.insert(k1);
    s.insert(k0); // double insert on purpose

    REQUIRE(s.size() == 2);
    REQUIRE(s.find(k0) != s.end());
    REQUIRE(s.find(k1) != s.end());

    Key k2, k3;
    init_key(k2, 0, 0);
    init_key(k3, 1, 1);
    REQUIRE(s.find(k2) == s.end());
    REQUIRE(s.find(k3) == s.end());

    destroy_key(k0);
    destroy_key(k1);
    destroy_key(k2);
    destroy_key(k3);
}

TEST_CASE("hash", "[hash]")
{
    SECTION("ordered-pair")
    {
        logger().info("Testing hashes of ordered std::pair");
        test_ordered<std::pair<int, unsigned>>(); // warning if cross-comparison
    }

    SECTION("ordered-array")
    {
        logger().info("Testing hashes of ordered std::array<T, 2>");
        test_ordered<std::array<int, 2>>();
    }

    SECTION("ordered-eigen-col-vector")
    {
        logger().info("Testing hashes of ordered Eigen 2D column vectors");
        test_ordered<Eigen::Matrix<int, 2, 1>>();
    }

    SECTION("ordered-eigen-row-vector")
    {
        logger().info("Testing hashes of ordered Eigen 2D row vectors");
        test_ordered<Eigen::Matrix<int, 1, 2>>();
    }
}

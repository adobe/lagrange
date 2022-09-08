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

#include <lagrange/Attribute.h>

#include <lagrange/AttributeTypes.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/utils/warning.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <catch2/catch_test_macros.hpp>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
#include <limits>
#include <numeric>

namespace {

template <typename E>
constexpr E invalid_enum()
{
    using ValueType = typename std::underlying_type<E>::type;
    return static_cast<E>(std::numeric_limits<ValueType>::max());
}

template <typename ValueType>
lagrange::Attribute<ValueType> make_attr(
    lagrange::AttributeElement element,
    lagrange::AttributeUsage usage,
    size_t num_channels,
    size_t num_elements = 0)
{
    lagrange::Attribute<ValueType> attr(element, usage, num_channels);
    if (num_elements != 0) {
        attr.resize_elements(num_elements);
        auto elems = attr.ref_all();
        std::iota(elems.begin(), elems.end(), ValueType(0));
    }
    // Attribute move/copy constructor is marked as explicit.
    return lagrange::Attribute<ValueType>(std::move(attr));
}

template <typename S, typename I>
lagrange::IndexedAttribute<S, I> make_indexed_attr(
    lagrange::AttributeUsage usage,
    size_t num_channels,
    size_t num_indices,
    size_t num_values)
{
    lagrange::IndexedAttribute<S, I> attr(usage, num_channels);
    if (num_indices != 0) {
        attr.indices().resize_elements(num_indices);
        auto indices = attr.indices().ref_all();
        std::iota(indices.begin(), indices.end(), I(0));
    }
    if (num_values != 0) {
        attr.values().resize_elements(num_values);
        auto values = attr.values().ref_all();
        std::iota(values.begin(), values.end(), S(0));
    }
    // Attribute move/copy constructor is marked as explicit.
    return lagrange::IndexedAttribute<S, I>(std::move(attr));
}

template <typename ValueType>
void test_create_attribute()
{
    using namespace lagrange;
    // Valid
    make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::Vector, 3);
    make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::Scalar, 1);
    make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::Normal, 1);
    make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::Normal, 3);
    make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::Normal, 5);
    make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::Color, 3);
    make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::UV, 2);

    // Valid only for integral types
    if (std::is_integral_v<ValueType>) {
        make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::VertexIndex, 1);
        make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::FacetIndex, 1);
        make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::CornerIndex, 1);
        make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::EdgeIndex, 1);
    } else {
        LA_REQUIRE_THROWS(
            make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::VertexIndex, 1));
        LA_REQUIRE_THROWS(
            make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::FacetIndex, 1));
        LA_REQUIRE_THROWS(
            make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::CornerIndex, 1));
        LA_REQUIRE_THROWS(
            make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::EdgeIndex, 1));
    }

    // Invalid
    LA_REQUIRE_THROWS(make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::Vector, 0));
    LA_REQUIRE_THROWS(make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::Scalar, 2));
    LA_REQUIRE_THROWS(make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::Color, 0));
    LA_REQUIRE_THROWS(make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::Color, 5));
    LA_REQUIRE_THROWS(make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::UV, 1));
    LA_REQUIRE_THROWS(make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::UV, 3));
    LA_REQUIRE_THROWS(
        make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::VertexIndex, 0));
    LA_REQUIRE_THROWS(
        make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::VertexIndex, 2));
    LA_REQUIRE_THROWS(
        make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::FacetIndex, 0));
    LA_REQUIRE_THROWS(
        make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::FacetIndex, 2));
    LA_REQUIRE_THROWS(
        make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::CornerIndex, 0));
    LA_REQUIRE_THROWS(
        make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::CornerIndex, 2));
    LA_REQUIRE_THROWS(make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::EdgeIndex, 0));
    LA_REQUIRE_THROWS(make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::EdgeIndex, 2));
    LA_REQUIRE_THROWS(
        make_attr<ValueType>(AttributeElement::Vertex, invalid_enum<AttributeUsage>(), 1));

    // Element type
    {
        auto attr = make_attr<ValueType>(AttributeElement::Facet, AttributeUsage::Vector, 3);
        REQUIRE(attr.get_element_type() == AttributeElement::Facet);
    }
}

template <typename S, typename I>
void test_move_copy_indexed()
{
    using namespace lagrange;
    const size_t num_channels = 3;
    const size_t num_indices = 4ull * 3ull;
    const size_t num_values = 10;

    // Move assignment operator to self
    {
        auto attr =
            make_indexed_attr<S, I>(AttributeUsage::Vector, num_channels, num_indices, num_values);
        void* old_indices = attr.indices().ref_all().data();
        void* old_values = attr.values().ref_all().data();
        LA_IGNORE_SELF_MOVE_WARNING_BEGIN
        attr = std::move(attr);
        LA_IGNORE_SELF_MOVE_WARNING_END
        void* new_indices = attr.indices().ref_all().data();
        void* new_values = attr.values().ref_all().data();
        REQUIRE(old_indices == new_indices);
        REQUIRE(old_values == new_values);
        REQUIRE(attr.values().ref_all().size() == num_values * num_channels);
        REQUIRE(attr.indices().ref_all().size() == num_indices);
    }

    // Move constructor to another variable
    {
        auto attr =
            make_indexed_attr<S, I>(AttributeUsage::Vector, num_channels, num_indices, num_values);
        void* old_indices = attr.indices().ref_all().data();
        void* old_values = attr.values().ref_all().data();
        lagrange::IndexedAttribute<S, I> new_attr(std::move(attr));
        void* new_indices = new_attr.indices().ref_all().data();
        void* new_values = new_attr.values().ref_all().data();
        REQUIRE(old_indices == new_indices);
        REQUIRE(old_values == new_values);
        REQUIRE(attr.values().ref_all().data() != old_values);
        REQUIRE(attr.values().ref_all().size() == 0);
        REQUIRE(attr.indices().ref_all().data() != old_indices);
        REQUIRE(attr.indices().ref_all().size() == 0);
    }

    // Move assignment operator to another variable
    {
        auto attr =
            make_indexed_attr<S, I>(AttributeUsage::Vector, num_channels, num_indices, num_values);
        void* old_indices = attr.indices().ref_all().data();
        void* old_values = attr.values().ref_all().data();
        lagrange::IndexedAttribute<S, I> new_attr(AttributeUsage::Scalar, 1);
        new_attr = std::move(attr);
        void* new_indices = new_attr.indices().ref_all().data();
        void* new_values = new_attr.values().ref_all().data();
        REQUIRE(old_indices == new_indices);
        REQUIRE(old_values == new_values);
        REQUIRE(attr.values().ref_all().data() != old_values);
        REQUIRE(attr.values().ref_all().size() == 0);
        REQUIRE(attr.indices().ref_all().data() != old_indices);
        REQUIRE(attr.indices().ref_all().size() == 0);
        REQUIRE(new_attr.get_num_channels() == num_channels);
        REQUIRE(new_attr.get_usage() == AttributeUsage::Vector);
    }

    // Copy assignment operator to self
    {
        auto attr =
            make_indexed_attr<S, I>(AttributeUsage::Vector, num_channels, num_indices, num_values);
        void* old_indices = attr.indices().ref_all().data();
        void* old_values = attr.values().ref_all().data();
        LA_IGNORE_SELF_ASSIGN_WARNING_BEGIN
        attr = attr;
        LA_IGNORE_SELF_ASSIGN_WARNING_END
        void* new_indices = attr.indices().ref_all().data();
        void* new_values = attr.values().ref_all().data();
        REQUIRE(old_indices == new_indices);
        REQUIRE(old_values == new_values);
        REQUIRE(attr.values().ref_all().size() == num_values * num_channels);
        REQUIRE(attr.indices().ref_all().size() == num_indices);
    }

    // Copy constructor to another variable
    {
        auto attr =
            make_indexed_attr<S, I>(AttributeUsage::Vector, num_channels, num_indices, num_values);
        void* old_indices = attr.indices().ref_all().data();
        void* old_values = attr.values().ref_all().data();
        lagrange::IndexedAttribute<S, I> new_attr(attr);
        void* new_indices = new_attr.indices().ref_all().data();
        void* new_values = new_attr.values().ref_all().data();
        REQUIRE(old_indices != new_indices);
        REQUIRE(old_values != new_values);
        REQUIRE(attr.values().ref_all().data() == old_values);
        REQUIRE(attr.indices().ref_all().data() == old_indices);
        REQUIRE(attr.values().ref_all().size() == num_values * num_channels);
        REQUIRE(attr.indices().ref_all().size() == num_indices);
    }

    // Copy assignment operator to another variable
    {
        auto attr =
            make_indexed_attr<S, I>(AttributeUsage::Vector, num_channels, num_indices, num_values);
        void* old_indices = attr.indices().ref_all().data();
        void* old_values = attr.values().ref_all().data();
        lagrange::IndexedAttribute<S, I> new_attr(AttributeUsage::Scalar, 1);
        new_attr = attr;
        void* new_indices = new_attr.indices().ref_all().data();
        void* new_values = new_attr.values().ref_all().data();
        REQUIRE(old_indices != new_indices);
        REQUIRE(old_values != new_values);
        REQUIRE(attr.values().ref_all().data() == old_values);
        REQUIRE(attr.indices().ref_all().data() == old_indices);
        REQUIRE(attr.values().ref_all().size() == num_values * num_channels);
        REQUIRE(attr.indices().ref_all().size() == num_indices);
        REQUIRE(new_attr.get_num_channels() == num_channels);
        REQUIRE(new_attr.get_usage() == AttributeUsage::Vector);
    }
}

template <typename ValueType>
void test_move_copy_internal()
{
    using namespace lagrange;
    const size_t num_channels = 3;
    const size_t num_elems = 10;

    // Move assignment operator to self
    {
        auto attr = make_attr<ValueType>(
            AttributeElement::Vertex,
            AttributeUsage::Vector,
            num_channels,
            num_elems);
        void* old_addr = attr.ref_all().data();
        LA_IGNORE_SELF_MOVE_WARNING_BEGIN
        attr = std::move(attr);
        LA_IGNORE_SELF_MOVE_WARNING_END
        void* new_addr = attr.ref_all().data();
        REQUIRE(old_addr == new_addr);
        REQUIRE(attr.ref_all().size() == num_elems * num_channels);
    }

    // Move constructor to another variable
    {
        auto attr = make_attr<ValueType>(
            AttributeElement::Vertex,
            AttributeUsage::Vector,
            num_channels,
            num_elems);
        void* old_addr = attr.ref_all().data();
        lagrange::Attribute<ValueType> new_attr(std::move(attr));
        void* new_addr = new_attr.ref_all().data();
        REQUIRE(old_addr == new_addr);
        REQUIRE(attr.ref_all().data() != old_addr);
        REQUIRE(attr.ref_all().size() == 0);
    }

    // Move assignment operator to another variable
    {
        auto attr = make_attr<ValueType>(
            AttributeElement::Vertex,
            AttributeUsage::Vector,
            num_channels,
            num_elems);
        void* old_addr = attr.ref_all().data();
        lagrange::Attribute<ValueType> new_attr(
            AttributeElement::Vertex,
            AttributeUsage::Scalar,
            1);
        new_attr = std::move(attr);
        void* new_addr = new_attr.ref_all().data();
        REQUIRE(old_addr == new_addr);
        REQUIRE(attr.ref_all().data() != old_addr);
        REQUIRE(attr.ref_all().size() == 0);
        REQUIRE(new_attr.get_num_channels() == num_channels);
        REQUIRE(new_attr.get_usage() == AttributeUsage::Vector);
    }

    // Copy assignment operator to self
    {
        auto attr = make_attr<ValueType>(
            AttributeElement::Vertex,
            AttributeUsage::Vector,
            num_channels,
            num_elems);
        void* old_addr = attr.ref_all().data();
        LA_IGNORE_SELF_ASSIGN_WARNING_BEGIN
        attr = attr;
        LA_IGNORE_SELF_ASSIGN_WARNING_END
        void* new_addr = attr.ref_all().data();
        REQUIRE(old_addr == new_addr);
        REQUIRE(attr.ref_all().size() == num_elems * num_channels);
    }

    // Copy constructor to another variable
    {
        auto attr = make_attr<ValueType>(
            AttributeElement::Vertex,
            AttributeUsage::Vector,
            num_channels,
            num_elems);
        void* old_addr = attr.ref_all().data();
        lagrange::Attribute<ValueType> new_attr(attr);
        void* new_addr = new_attr.ref_all().data();
        REQUIRE(old_addr != new_addr);
        REQUIRE(attr.ref_all().data() == old_addr);
        REQUIRE(attr.ref_all().size() == num_elems * num_channels);
    }

    // Copy assignment operator to another variable
    {
        auto attr = make_attr<ValueType>(
            AttributeElement::Vertex,
            AttributeUsage::Vector,
            num_channels,
            num_elems);
        void* old_addr = attr.ref_all().data();
        lagrange::Attribute<ValueType> new_attr(
            AttributeElement::Vertex,
            AttributeUsage::Scalar,
            1);
        new_attr = attr;
        void* new_addr = new_attr.ref_all().data();
        REQUIRE(old_addr != new_addr);
        REQUIRE(attr.ref_all().data() == old_addr);
        REQUIRE(attr.ref_all().size() == num_elems * num_channels);
        REQUIRE(new_attr.get_num_channels() == num_channels);
        REQUIRE(new_attr.get_usage() == AttributeUsage::Vector);
    }
}

template <typename ValueType>
void test_move_copy_external()
{
    using namespace lagrange;
    const size_t num_channels = 3;
    const size_t num_elems = 10;

    std::vector<ValueType> values(num_elems * num_channels);
    std::iota(values.begin(), values.end(), ValueType(0));

    // Move assignment operator to self
    {
        Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
        attr.wrap(values, num_elems);
        void* old_addr = attr.ref_all().data();
        REQUIRE(old_addr == values.data());
        LA_IGNORE_SELF_MOVE_WARNING_BEGIN
        attr = std::move(attr);
        LA_IGNORE_SELF_MOVE_WARNING_END
        void* new_addr = attr.ref_all().data();
        REQUIRE(old_addr == new_addr);
        REQUIRE(attr.ref_all().size() == num_elems * num_channels);
    }

    // Move constructor to another variable
    {
        Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
        attr.wrap(values, num_elems);
        void* old_addr = attr.ref_all().data();
        REQUIRE(old_addr == values.data());
        lagrange::Attribute<ValueType> new_attr(std::move(attr));
        REQUIRE(new_attr.is_external());
        REQUIRE(attr.is_external());
        void* new_addr = new_attr.ref_all().data();
        REQUIRE(old_addr == new_addr);
        REQUIRE(attr.ref_all().data() != old_addr);
        REQUIRE(attr.ref_all().size() == 0);
    }

    // Move assignment operator to another variable
    {
        Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
        attr.wrap(values, num_elems);
        void* old_addr = attr.ref_all().data();
        REQUIRE(old_addr == values.data());
        lagrange::Attribute<ValueType> new_attr(
            AttributeElement::Vertex,
            AttributeUsage::Scalar,
            1);
        new_attr = std::move(attr);
        void* new_addr = new_attr.ref_all().data();
        REQUIRE(old_addr == new_addr);
        REQUIRE(attr.ref_all().data() != old_addr);
        REQUIRE(attr.ref_all().size() == 0);
        REQUIRE(new_attr.get_num_channels() == num_channels);
        REQUIRE(new_attr.get_usage() == AttributeUsage::Vector);
    }

    // Copy assignment operator to self
    {
        Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
        attr.wrap(values, num_elems);
        void* old_addr = attr.ref_all().data();
        REQUIRE(old_addr == values.data());
        LA_IGNORE_SELF_ASSIGN_WARNING_BEGIN
        attr = attr;
        LA_IGNORE_SELF_ASSIGN_WARNING_END
        void* new_addr = attr.ref_all().data();
        REQUIRE(old_addr == new_addr);
        REQUIRE(attr.ref_all().size() == num_elems * num_channels);
    }

    // Copy constructor to another variable
    {
        Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
        attr.wrap(values, num_elems);
        void* old_addr = attr.ref_all().data();
        REQUIRE(old_addr == values.data());
        lagrange::Attribute<ValueType> new_attr(attr);
        void* new_addr = new_attr.ref_all().data();
        REQUIRE(old_addr == new_addr);
        REQUIRE(attr.ref_all().data() == old_addr);
        REQUIRE(attr.ref_all().size() == num_elems * num_channels);
    }

    // Copy assignment operator to another variable
    {
        Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
        attr.wrap(values, num_elems);
        void* old_addr = attr.ref_all().data();
        REQUIRE(old_addr == values.data());
        lagrange::Attribute<ValueType> new_attr(
            AttributeElement::Vertex,
            AttributeUsage::Scalar,
            1);
        new_attr = attr;
        void* new_addr = new_attr.ref_all().data();
        REQUIRE(old_addr == new_addr);
        REQUIRE(attr.ref_all().data() == old_addr);
        REQUIRE(attr.ref_all().size() == num_elems * num_channels);
        REQUIRE(new_attr.get_num_channels() == num_channels);
        REQUIRE(new_attr.get_usage() == AttributeUsage::Vector);
    }
}

template <typename ValueType>
void test_data_access()
{
    using namespace lagrange;
    const size_t num_channels = 3;
    const size_t num_elems = 10;

    std::vector<ValueType> values(num_elems * num_channels);
    std::iota(values.begin(), values.end(), ValueType(0));

    // Internal data (vector)
    {
        auto attr = make_attr<ValueType>(
            AttributeElement::Vertex,
            AttributeUsage::Vector,
            num_channels,
            num_elems);
        LA_REQUIRE_THROWS(attr.get(0));
        LA_REQUIRE_THROWS(attr.ref(0));
        for (size_t i = 0; i < num_elems; ++i) {
            for (size_t j = 0; j < num_channels; ++j) {
                REQUIRE(attr.get(i, j) == values[i * num_channels + j]);
            }
        }
        for (auto& x : attr.ref_all()) {
            x = ValueType(x * 3);
        }
        for (size_t i = 0; i < num_elems; ++i) {
            for (size_t j = 0; j < num_channels; ++j) {
                REQUIRE(attr.get(i, j) == ValueType(3 * values[i * num_channels + j]));
            }
        }
        for (size_t i = 0; i < num_elems; ++i) {
            for (size_t j = 0; j < num_channels; ++j) {
                ValueType new_x = ValueType(5 * values[i * num_channels + j]);
                attr.ref(i, j) = new_x;
                REQUIRE(attr.get(i, j) == new_x);
            }
        }
    }

    // Internal data (scalar or 1d vector)
    for (auto usage : {AttributeUsage::Scalar, AttributeUsage::Vector}) {
        auto attr = make_attr<ValueType>(AttributeElement::Vertex, usage, 1, num_elems);
        REQUIRE_NOTHROW(attr.get(0, 0));
        REQUIRE_NOTHROW(attr.ref(0, 0));
        for (size_t i = 0; i < num_elems; ++i) {
            REQUIRE(attr.get(i) == values[i]);
        }
        for (auto& x : attr.ref_all()) {
            x = ValueType(x * 3);
        }
        for (size_t i = 0; i < num_elems; ++i) {
            REQUIRE(attr.get(i) == ValueType(3 * values[i]));
        }
        for (size_t i = 0; i < num_elems; ++i) {
            ValueType new_x = ValueType(5 * values[i]);
            attr.ref(i) = new_x;
            REQUIRE(attr.get(i) == new_x);
        }
    }

    // External data (vector)
    {
        Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
        auto copy = values;
        attr.wrap(copy, num_elems);
        LA_REQUIRE_THROWS(attr.get(0));
        LA_REQUIRE_THROWS(attr.ref(0));
        for (size_t i = 0; i < num_elems; ++i) {
            for (size_t j = 0; j < num_channels; ++j) {
                REQUIRE(attr.get(i, j) == values[i * num_channels + j]);
            }
        }
        for (auto& x : attr.ref_all()) {
            x = ValueType(x * 3);
        }
        for (size_t i = 0; i < num_elems; ++i) {
            for (size_t j = 0; j < num_channels; ++j) {
                REQUIRE(attr.get(i, j) == ValueType(3 * values[i * num_channels + j]));
                REQUIRE(attr.get(i, j) == copy[i * num_channels + j]);
            }
        }
        for (size_t i = 0; i < num_elems; ++i) {
            for (size_t j = 0; j < num_channels; ++j) {
                ValueType new_x = ValueType(5 * values[i * num_channels + j]);
                attr.ref(i, j) = new_x;
                REQUIRE(attr.get(i, j) == new_x);
                REQUIRE(attr.get(i, j) == copy[i * num_channels + j]);
            }
        }
    }

    // External data (scalar or 1d vector)
    for (auto usage : {AttributeUsage::Scalar, AttributeUsage::Vector}) {
        Attribute<ValueType> attr(AttributeElement::Vertex, usage, 1);
        auto copy = values;
        attr.wrap(copy, num_elems);
        REQUIRE_NOTHROW(attr.get(0, 0));
        REQUIRE_NOTHROW(attr.ref(0, 0));
        for (size_t i = 0; i < num_elems; ++i) {
            REQUIRE(attr.get(i) == values[i]);
        }
        for (auto& x : attr.ref_all()) {
            x = ValueType(x * 3);
        }
        for (size_t i = 0; i < num_elems; ++i) {
            REQUIRE(attr.get(i) == ValueType(3 * values[i]));
            REQUIRE(attr.get(i) == copy[i]);
        }
        for (size_t i = 0; i < num_elems; ++i) {
            ValueType new_x = ValueType(5 * values[i]);
            attr.ref(i) = new_x;
            REQUIRE(attr.get(i) == new_x);
            REQUIRE(attr.get(i) == copy[i]);
        }
    }
}

template <typename ValueType>
void test_write_policy()
{
    using namespace lagrange;
    const size_t num_channels = 3;
    const size_t num_elems = 10;

    std::vector<ValueType> values(num_elems * num_channels);
    std::iota(values.begin(), values.end(), ValueType(0));

    // Wrap const data + data access operation [default policy]
    {
        Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
        REQUIRE(!attr.is_read_only());
        REQUIRE(!attr.is_external());
        attr.wrap_const(values, num_elems);
        REQUIRE(attr.is_read_only());
        REQUIRE(attr.is_external());
        REQUIRE(attr.get_write_policy() == AttributeWritePolicy::ErrorIfReadOnly);

        // Write access should raise an exception
        LA_REQUIRE_THROWS(attr.ref(0, 0) = ValueType(10));
        LA_REQUIRE_THROWS(attr.ref_all());
        LA_REQUIRE_THROWS(attr.ref_first(1));
        LA_REQUIRE_THROWS(attr.ref_last(1));
        LA_REQUIRE_THROWS(attr.ref_middle(1, 2));

        // To avoid raising an exception/accessing an invalid span, request const pointers explicitly
        REQUIRE_NOTHROW(attr.get_all());
        REQUIRE_NOTHROW(attr.get_first(1));
        REQUIRE_NOTHROW(attr.get_last(1));
        REQUIRE_NOTHROW(attr.get_middle(1, 2));
    }

    // Wrap const data + data access operation [silent copy policy]
    {
        Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
        attr.wrap_const(values, num_elems);
        attr.set_write_policy(AttributeWritePolicy::SilentCopy);
        const void* old_addr = attr.get_all().data();
        REQUIRE(old_addr == values.data());

        // Via .ref()
        {
            auto new_attr = Attribute<ValueType>(attr);
            REQUIRE_NOTHROW(new_attr.ref(0, 0) = ValueType(10));
            const void* new_addr = new_attr.get_all().data();
            REQUIRE(old_addr != new_addr);
            REQUIRE(old_addr == attr.get_all().data());
        }

        // Via .ref_all()
        {
            auto new_attr = Attribute<ValueType>(attr);
            REQUIRE_NOTHROW(new_attr.ref_all());
            const void* new_addr = new_attr.get_all().data();
            REQUIRE(old_addr != new_addr);
            REQUIRE(old_addr == attr.get_all().data());
        }
    }

    // Wrap const data + data access operation [warn and copy policy]
    {
        Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
        attr.wrap_const(values, num_elems);
        attr.set_write_policy(AttributeWritePolicy::WarnAndCopy);
        const void* old_addr = attr.get_all().data();
        REQUIRE(old_addr == values.data());

        // Via .ref()
        {
            auto new_attr = Attribute<ValueType>(attr);
            REQUIRE_NOTHROW(new_attr.ref(0, 0) = ValueType(10));
            const void* new_addr = new_attr.get_all().data();
            REQUIRE(old_addr != new_addr);
            REQUIRE(old_addr == attr.get_all().data());
        }

        // Via .ref_all()
        {
            auto new_attr = Attribute<ValueType>(attr);
            REQUIRE_NOTHROW(new_attr.ref_all());
            const void* new_addr = new_attr.get_all().data();
            REQUIRE(old_addr != new_addr);
            REQUIRE(old_addr == attr.get_all().data());
        }
    }

    // Garbage policy
    {
        Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
        attr.wrap_const(values, num_elems);
        attr.set_write_policy(invalid_enum<AttributeWritePolicy>());
        const void* old_addr = attr.get_all().data();
        REQUIRE(old_addr == values.data());
        LA_REQUIRE_THROWS(attr.ref_all());
    }
}

template <typename ValueType>
void test_growth_policy()
{
    using namespace lagrange;
    const size_t num_channels = 3;
    const size_t num_elems = 5;
    const size_t max_elems = 9;
    const size_t delta_elems = max_elems - num_elems;

    std::vector<ValueType> values(max_elems * num_channels);
    std::iota(values.begin(), values.end(), ValueType(0));

    // ErrorIfExternal
    {{Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
    attr.wrap(values, num_elems);
    REQUIRE(attr.get_growth_policy() == AttributeGrowthPolicy::ErrorIfExternal);
    LA_REQUIRE_THROWS(
        attr.insert_elements({values.data() + num_channels * num_elems, num_channels * 1}));
    LA_REQUIRE_THROWS(attr.insert_elements(1));
}

{
    Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
    attr.wrap(values, num_elems);
    REQUIRE_NOTHROW(attr.reserve_entries(num_elems * num_channels));
    LA_REQUIRE_THROWS(attr.reserve_entries((num_elems + 1) * num_channels));
}

{
    Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
    attr.wrap(values, num_elems);
    REQUIRE_NOTHROW(attr.resize_elements(num_elems));
    LA_REQUIRE_THROWS(attr.resize_elements(num_elems - 1));
    LA_REQUIRE_THROWS(attr.resize_elements(num_elems + 1));
}

{
    Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
    attr.wrap(values, num_elems);
    LA_REQUIRE_THROWS(attr.clear());
}
} // namespace

// AllowWithinCapacity
{{Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
attr.wrap(values, num_elems);
attr.set_growth_policy(AttributeGrowthPolicy::AllowWithinCapacity);

REQUIRE_NOTHROW(
    attr.insert_elements({values.data() + num_channels * num_elems, num_channels* delta_elems}));
REQUIRE(attr.get_num_elements() == max_elems);
LA_REQUIRE_THROWS(attr.insert_elements(1));
REQUIRE(attr.get_all().data() == values.data());
REQUIRE(attr.ref_all().data() == values.data());
for (size_t i = 0; i < values.size(); ++i) {
    REQUIRE(values[i] == ValueType(i));
}
}

{
    Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
    attr.wrap(values, num_elems);
    attr.set_growth_policy(AttributeGrowthPolicy::AllowWithinCapacity);
    attr.set_default_value(ValueType(10));
    REQUIRE_NOTHROW(attr.insert_elements(delta_elems));
    for (size_t i = 0; i < values.size(); ++i) {
        REQUIRE(values[i] == (i < num_elems * num_channels ? ValueType(i) : ValueType(10)));
    }
    std::iota(values.begin(), values.end(), ValueType(0));
    LA_REQUIRE_THROWS(attr.insert_elements(1));
    REQUIRE(attr.ref_all().data() == values.data());
    for (size_t i = 0; i < values.size(); ++i) {
        REQUIRE(values[i] == ValueType(i));
    }
}

{
    Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
    attr.wrap(values, num_elems);
    attr.set_growth_policy(AttributeGrowthPolicy::AllowWithinCapacity);
    REQUIRE_NOTHROW(attr.reserve_entries(max_elems * num_channels));
    LA_REQUIRE_THROWS(attr.reserve_entries((max_elems + 1) * num_channels));
    REQUIRE(attr.ref_all().data() == values.data());
    for (size_t i = 0; i < values.size(); ++i) {
        REQUIRE(values[i] == ValueType(i));
    }
}

{
    Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
    attr.wrap(values, num_elems);
    attr.set_growth_policy(AttributeGrowthPolicy::AllowWithinCapacity);
    REQUIRE_NOTHROW(attr.resize_elements(max_elems));
    LA_REQUIRE_THROWS(attr.resize_elements(max_elems + 1));
    REQUIRE(attr.ref_all().data() == values.data());
    for (size_t i = 0; i < values.size(); ++i) {
        REQUIRE(values[i] == (i < num_elems * num_channels ? ValueType(i) : ValueType(0)));
    }
    std::iota(values.begin(), values.end(), ValueType(0));
}

{
    Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
    attr.wrap(values, num_elems);
    attr.set_growth_policy(AttributeGrowthPolicy::AllowWithinCapacity);
    REQUIRE_NOTHROW(attr.clear());
    REQUIRE(attr.ref_all().data() == values.data());
    for (size_t i = 0; i < values.size(); ++i) {
        REQUIRE(values[i] == ValueType(i));
    }
}
}

// SilentCopy
{{Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
attr.wrap(values, num_elems);
attr.set_growth_policy(AttributeGrowthPolicy::SilentCopy);
for (size_t i = 0; i < values.size(); ++i) {
    REQUIRE(values[i] == ValueType(i));
}

REQUIRE_NOTHROW(
    attr.insert_elements({values.data() + num_channels * num_elems, num_channels* delta_elems}));
REQUIRE(attr.get_num_elements() == max_elems);
REQUIRE_NOTHROW(attr.insert_elements(1));
REQUIRE(attr.get_all().data() != values.data());
REQUIRE(attr.ref_all().data() != values.data());
for (size_t i = 0; i < values.size(); ++i) {
    REQUIRE(values[i] == ValueType(i));
}
}

{
    Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
    attr.wrap(values, num_elems);
    attr.set_growth_policy(AttributeGrowthPolicy::SilentCopy);
    REQUIRE_NOTHROW(attr.insert_elements(delta_elems));
    REQUIRE_NOTHROW(attr.insert_elements(1));
    REQUIRE(attr.ref_all().data() != values.data());
}

{
    Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
    attr.wrap(values, num_elems);
    attr.set_growth_policy(AttributeGrowthPolicy::SilentCopy);
    REQUIRE_NOTHROW(attr.reserve_entries(max_elems * num_channels));
    REQUIRE_NOTHROW(attr.reserve_entries((max_elems + 1) * num_channels));
    REQUIRE(attr.ref_all().data() != values.data());
}

{
    Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
    attr.wrap(values, num_elems);
    attr.set_growth_policy(AttributeGrowthPolicy::SilentCopy);
    REQUIRE_NOTHROW(attr.resize_elements(max_elems));
    REQUIRE_NOTHROW(attr.resize_elements(max_elems + 1));
    REQUIRE(attr.ref_all().data() != values.data());
}

{
    Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
    attr.wrap(values, num_elems);
    attr.set_growth_policy(AttributeGrowthPolicy::SilentCopy);
    REQUIRE_NOTHROW(attr.clear());
    REQUIRE(attr.ref_all().data() != values.data());
}
}

// WarnAndCopy
{{Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
attr.wrap(values, num_elems);
attr.set_growth_policy(AttributeGrowthPolicy::WarnAndCopy);

REQUIRE_NOTHROW(
    attr.insert_elements({values.data() + num_channels * num_elems, num_channels* delta_elems}));
REQUIRE(attr.get_num_elements() == max_elems);
REQUIRE_NOTHROW(attr.insert_elements(1));
REQUIRE(attr.get_all().data() != values.data());
REQUIRE(attr.ref_all().data() != values.data());
for (size_t i = 0; i < values.size(); ++i) {
    REQUIRE(values[i] == ValueType(i));
}
}

{
    Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
    attr.wrap(values, num_elems);
    attr.set_growth_policy(AttributeGrowthPolicy::WarnAndCopy);
    REQUIRE_NOTHROW(attr.insert_elements(delta_elems));
    REQUIRE_NOTHROW(attr.insert_elements(1));
    REQUIRE(attr.ref_all().data() != values.data());
}

{
    Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
    attr.wrap(values, num_elems);
    attr.set_growth_policy(AttributeGrowthPolicy::WarnAndCopy);
    REQUIRE_NOTHROW(attr.reserve_entries(max_elems * num_channels));
    REQUIRE_NOTHROW(attr.reserve_entries((max_elems + 1) * num_channels));
    REQUIRE(attr.ref_all().data() != values.data());
}

{
    Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
    attr.wrap(values, num_elems);
    attr.set_growth_policy(AttributeGrowthPolicy::WarnAndCopy);
    REQUIRE_NOTHROW(attr.resize_elements(max_elems));
    REQUIRE_NOTHROW(attr.resize_elements(max_elems + 1));
    REQUIRE(attr.ref_all().data() != values.data());
}

{
    Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
    attr.wrap(values, num_elems);
    attr.set_growth_policy(AttributeGrowthPolicy::WarnAndCopy);
    REQUIRE_NOTHROW(attr.clear());
    REQUIRE(attr.ref_all().data() != values.data());
}
}

// Garbage policy
{
    Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
    attr.wrap(values, num_elems);
    attr.set_growth_policy(invalid_enum<AttributeGrowthPolicy>());
    LA_REQUIRE_THROWS(attr.insert_elements(1));
}
}

template <typename ValueType>
void test_empty_buffers()
{
    using namespace lagrange;
    const size_t num_channels = 3;
    const size_t num_elems = 0;

    std::vector<ValueType> values(num_elems * num_channels);
    std::iota(values.begin(), values.end(), ValueType(0));

    {
        auto attr = make_attr<ValueType>(
            AttributeElement::Vertex,
            AttributeUsage::Vector,
            num_channels,
            num_elems);
        REQUIRE(!attr.is_external());
        REQUIRE(!attr.is_read_only());
    }

    {
        Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
        attr.wrap(values, num_elems);
        REQUIRE(attr.is_external());
        REQUIRE(!attr.is_read_only());
        REQUIRE(attr.ref_all().data() == values.data());
    }

    {
        Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Vector, num_channels);
        attr.wrap_const(values, num_elems);
        REQUIRE(attr.is_external());
        REQUIRE(attr.is_read_only());
        REQUIRE(attr.get_all().data() == values.data());
        LA_REQUIRE_THROWS(attr.ref_all());
    }
}

} // namespace

TEST_CASE("IndexedAttribute: Move-Copy", "[next]")
{
#define LA_X_move_copy_indexed_values(I, S) test_move_copy_indexed<S, I>();
#define LA_X_move_copy_indexed_indices(_, I) LA_ATTRIBUTE_X(move_copy_indexed_values, I)
    LA_SURFACE_MESH_INDEX_X(move_copy_indexed_indices, 0)
}

TEST_CASE("Attribute: Create", "[next]")
{
#define LA_X_create_attribute(_, ValueType) test_create_attribute<ValueType>();
    LA_ATTRIBUTE_X(create_attribute, 0)
}

TEST_CASE("Attribute: Move-Copy Internal", "[next]")
{
#define LA_X_move_copy_internal(_, ValueType) test_move_copy_internal<ValueType>();
    LA_ATTRIBUTE_X(move_copy_internal, 0)
}

TEST_CASE("Attribute: Move-Copy External", "[next]")
{
#define LA_X_move_copy_external(_, ValueType) test_move_copy_external<ValueType>();
    LA_ATTRIBUTE_X(move_copy_external, 0)
}

TEST_CASE("Attribute: Data Access", "[next]")
{
#define LA_X_data_access(_, ValueType) test_data_access<ValueType>();
    LA_ATTRIBUTE_X(data_access, 0)
}

TEST_CASE("Attribute: Write Policy", "[next]")
{
#define LA_X_write_policy(_, ValueType) test_write_policy<ValueType>();
    LA_ATTRIBUTE_X(write_policy, 0)
}

TEST_CASE("Attribute: Growth Policy", "[next]")
{
#define LA_X_growth_policy(_, ValueType) test_growth_policy<ValueType>();
    LA_ATTRIBUTE_X(growth_policy, 0)
}

TEST_CASE("Attribute: Empty Buffers", "[next]")
{
#define LA_X_empty_buffers(_, ValueType) test_empty_buffers<ValueType>();
    LA_ATTRIBUTE_X(empty_buffers, 0)
}

// TODO
// - Test reserve entries with external buffer (const and non-const)
// - Test resize elements with external buffer (decrease, equal, and increase capacity) for all
//   policies
// - Test insert elements with external buffer for all policies
// - Test default value + attribute growth

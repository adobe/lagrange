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
    // Return by value is possible because the move constructor is not explicit.
    return attr;
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
    // Return by value is possible because the move constructor is not explicit.
    return attr;
}

template <typename ValueType>
void test_create_attribute()
{
    using namespace lagrange;
    // Valid
    make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::Vector, 3);
    make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::Scalar, 1);
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

    // Valid only for floating point types
    for (auto usage :
         {AttributeUsage::Position,
          AttributeUsage::Normal,
          AttributeUsage::Tangent,
          AttributeUsage::Bitangent}) {
        if (std::is_floating_point_v<ValueType>) {
            make_attr<ValueType>(AttributeElement::Vertex, usage, 1);
            make_attr<ValueType>(AttributeElement::Vertex, usage, 3);
            make_attr<ValueType>(AttributeElement::Vertex, usage, 5);
        } else {
            LA_REQUIRE_THROWS(make_attr<ValueType>(AttributeElement::Vertex, usage, 1));
            LA_REQUIRE_THROWS(make_attr<ValueType>(AttributeElement::Vertex, usage, 3));
            LA_REQUIRE_THROWS(make_attr<ValueType>(AttributeElement::Vertex, usage, 5));
        }
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
        attr.set_copy_policy(AttributeCopyPolicy::KeepExternalPtr);
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
        attr.set_copy_policy(AttributeCopyPolicy::KeepExternalPtr);
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
    {
        {
            Attribute<ValueType> attr(
                AttributeElement::Vertex,
                AttributeUsage::Vector,
                num_channels);
            attr.wrap(values, num_elems);
            REQUIRE(attr.get_growth_policy() == AttributeGrowthPolicy::ErrorIfExternal);
            LA_REQUIRE_THROWS(
                attr.insert_elements({values.data() + num_channels * num_elems, num_channels * 1}));
            LA_REQUIRE_THROWS(attr.insert_elements(1));
        }

        {
            Attribute<ValueType> attr(
                AttributeElement::Vertex,
                AttributeUsage::Vector,
                num_channels);
            attr.wrap(values, num_elems);
            REQUIRE_NOTHROW(attr.reserve_entries(num_elems * num_channels));
            LA_REQUIRE_THROWS(attr.reserve_entries((num_elems + 1) * num_channels));
        }

        {
            Attribute<ValueType> attr(
                AttributeElement::Vertex,
                AttributeUsage::Vector,
                num_channels);
            attr.wrap(values, num_elems);
            REQUIRE_NOTHROW(attr.resize_elements(num_elems));
            LA_REQUIRE_THROWS(attr.resize_elements(num_elems - 1));
            LA_REQUIRE_THROWS(attr.resize_elements(num_elems + 1));
        }

        {
            Attribute<ValueType> attr(
                AttributeElement::Vertex,
                AttributeUsage::Vector,
                num_channels);
            attr.wrap(values, num_elems);
            LA_REQUIRE_THROWS(attr.clear());
        }
    } // namespace

    // AllowWithinCapacity
    {
        {
            Attribute<ValueType> attr(
                AttributeElement::Vertex,
                AttributeUsage::Vector,
                num_channels);
            attr.wrap(values, num_elems);
            attr.set_growth_policy(AttributeGrowthPolicy::AllowWithinCapacity);

            REQUIRE_NOTHROW(attr.insert_elements(
                {values.data() + num_channels * num_elems, num_channels * delta_elems}));
            REQUIRE(attr.get_num_elements() == max_elems);
            LA_REQUIRE_THROWS(attr.insert_elements(1));
            REQUIRE(attr.get_all().data() == values.data());
            REQUIRE(attr.ref_all().data() == values.data());
            for (size_t i = 0; i < values.size(); ++i) {
                REQUIRE(values[i] == ValueType(i));
            }
        }

        {
            Attribute<ValueType> attr(
                AttributeElement::Vertex,
                AttributeUsage::Vector,
                num_channels);
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
            Attribute<ValueType> attr(
                AttributeElement::Vertex,
                AttributeUsage::Vector,
                num_channels);
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
            Attribute<ValueType> attr(
                AttributeElement::Vertex,
                AttributeUsage::Vector,
                num_channels);
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
            Attribute<ValueType> attr(
                AttributeElement::Vertex,
                AttributeUsage::Vector,
                num_channels);
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
    {
        {
            Attribute<ValueType> attr(
                AttributeElement::Vertex,
                AttributeUsage::Vector,
                num_channels);
            attr.wrap(values, num_elems);
            attr.set_growth_policy(AttributeGrowthPolicy::SilentCopy);
            for (size_t i = 0; i < values.size(); ++i) {
                REQUIRE(values[i] == ValueType(i));
            }

            REQUIRE_NOTHROW(attr.insert_elements(
                {values.data() + num_channels * num_elems, num_channels * delta_elems}));
            REQUIRE(attr.get_num_elements() == max_elems);
            REQUIRE_NOTHROW(attr.insert_elements(1));
            REQUIRE(attr.get_all().data() != values.data());
            REQUIRE(attr.ref_all().data() != values.data());
            for (size_t i = 0; i < values.size(); ++i) {
                REQUIRE(values[i] == ValueType(i));
            }
        }

        {
            Attribute<ValueType> attr(
                AttributeElement::Vertex,
                AttributeUsage::Vector,
                num_channels);
            attr.wrap(values, num_elems);
            attr.set_growth_policy(AttributeGrowthPolicy::SilentCopy);
            REQUIRE_NOTHROW(attr.insert_elements(delta_elems));
            REQUIRE_NOTHROW(attr.insert_elements(1));
            REQUIRE(attr.ref_all().data() != values.data());
        }

        {
            Attribute<ValueType> attr(
                AttributeElement::Vertex,
                AttributeUsage::Vector,
                num_channels);
            attr.wrap(values, num_elems);
            attr.set_growth_policy(AttributeGrowthPolicy::SilentCopy);
            REQUIRE_NOTHROW(attr.reserve_entries(max_elems * num_channels));
            REQUIRE_NOTHROW(attr.reserve_entries((max_elems + 1) * num_channels));
            REQUIRE(attr.ref_all().data() != values.data());
        }

        {
            Attribute<ValueType> attr(
                AttributeElement::Vertex,
                AttributeUsage::Vector,
                num_channels);
            attr.wrap(values, num_elems);
            attr.set_growth_policy(AttributeGrowthPolicy::SilentCopy);
            REQUIRE_NOTHROW(attr.resize_elements(max_elems));
            REQUIRE_NOTHROW(attr.resize_elements(max_elems + 1));
            REQUIRE(attr.ref_all().data() != values.data());
        }

        {
            Attribute<ValueType> attr(
                AttributeElement::Vertex,
                AttributeUsage::Vector,
                num_channels);
            attr.wrap(values, num_elems);
            attr.set_growth_policy(AttributeGrowthPolicy::SilentCopy);
            REQUIRE_NOTHROW(attr.clear());
            REQUIRE(attr.ref_all().data() != values.data());
        }
    }

    // WarnAndCopy
    {
        {
            Attribute<ValueType> attr(
                AttributeElement::Vertex,
                AttributeUsage::Vector,
                num_channels);
            attr.wrap(values, num_elems);
            attr.set_growth_policy(AttributeGrowthPolicy::WarnAndCopy);

            REQUIRE_NOTHROW(attr.insert_elements(
                {values.data() + num_channels * num_elems, num_channels * delta_elems}));
            REQUIRE(attr.get_num_elements() == max_elems);
            REQUIRE_NOTHROW(attr.insert_elements(1));
            REQUIRE(attr.get_all().data() != values.data());
            REQUIRE(attr.ref_all().data() != values.data());
            for (size_t i = 0; i < values.size(); ++i) {
                REQUIRE(values[i] == ValueType(i));
            }
        }

        {
            Attribute<ValueType> attr(
                AttributeElement::Vertex,
                AttributeUsage::Vector,
                num_channels);
            attr.wrap(values, num_elems);
            attr.set_growth_policy(AttributeGrowthPolicy::WarnAndCopy);
            REQUIRE_NOTHROW(attr.insert_elements(delta_elems));
            REQUIRE_NOTHROW(attr.insert_elements(1));
            REQUIRE(attr.ref_all().data() != values.data());
        }

        {
            Attribute<ValueType> attr(
                AttributeElement::Vertex,
                AttributeUsage::Vector,
                num_channels);
            attr.wrap(values, num_elems);
            attr.set_growth_policy(AttributeGrowthPolicy::WarnAndCopy);
            REQUIRE_NOTHROW(attr.reserve_entries(max_elems * num_channels));
            REQUIRE_NOTHROW(attr.reserve_entries((max_elems + 1) * num_channels));
            REQUIRE(attr.ref_all().data() != values.data());
        }

        {
            Attribute<ValueType> attr(
                AttributeElement::Vertex,
                AttributeUsage::Vector,
                num_channels);
            attr.wrap(values, num_elems);
            attr.set_growth_policy(AttributeGrowthPolicy::WarnAndCopy);
            REQUIRE_NOTHROW(attr.resize_elements(max_elems));
            REQUIRE_NOTHROW(attr.resize_elements(max_elems + 1));
            REQUIRE(attr.ref_all().data() != values.data());
        }

        {
            Attribute<ValueType> attr(
                AttributeElement::Vertex,
                AttributeUsage::Vector,
                num_channels);
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

template <typename ValueType>
void test_ownership()
{
    using namespace lagrange;
    const size_t num_channels = 1;
    const size_t num_elements = 3;

    using Buffer = std::vector<ValueType>;

    auto data = std::make_shared<Buffer>(num_elements, ValueType(0));
    auto buffer_ptr = make_shared_span(data, data->data(), data->size());
    auto owner = buffer_ptr.owner();

    REQUIRE(owner.use_count() == 3);
    {
        Attribute<ValueType> attr(AttributeElement::Vertex, AttributeUsage::Scalar, num_channels);
        attr.wrap(buffer_ptr, num_elements);
        REQUIRE(owner.use_count() == 4);
        REQUIRE(attr.get_all().data() == owner.get());

        Attribute<ValueType> another_attr(
            AttributeElement::Vertex,
            AttributeUsage::Scalar,
            num_channels);
        another_attr.wrap(buffer_ptr, num_elements);
        REQUIRE(owner.use_count() == 5);
        REQUIRE(another_attr.get_all().data() == owner.get());
    }
    REQUIRE(owner.use_count() == 3);
}

template <typename ValueType>
void test_policy_propagation()
{
    using namespace lagrange;

    auto make_source = []() {
        auto attr = make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::Vector, 3, 2);
        attr.set_growth_policy(AttributeGrowthPolicy::AllowWithinCapacity);
        attr.set_shrink_policy(AttributeShrinkPolicy::IgnoreIfExternal);
        attr.set_write_policy(AttributeWritePolicy::SilentCopy);
        attr.set_copy_policy(AttributeCopyPolicy::KeepExternalPtr);
        attr.set_cast_policy(AttributeCastPolicy::DoNotRemapInvalid);
        return attr;
    };
    auto check = [](const Attribute<ValueType>& attr) {
        REQUIRE(attr.get_growth_policy() == AttributeGrowthPolicy::AllowWithinCapacity);
        REQUIRE(attr.get_shrink_policy() == AttributeShrinkPolicy::IgnoreIfExternal);
        REQUIRE(attr.get_write_policy() == AttributeWritePolicy::SilentCopy);
        REQUIRE(attr.get_copy_policy() == AttributeCopyPolicy::KeepExternalPtr);
        REQUIRE(attr.get_cast_policy() == AttributeCastPolicy::DoNotRemapInvalid);
    };

    {
        auto source = make_source();
        Attribute<ValueType> copied(source);
        check(copied);
    }
    {
        auto source = make_source();
        auto target = make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::Vector, 3, 1);
        target = source;
        check(target);
    }
    {
        auto source = make_source();
        Attribute<ValueType> moved(std::move(source));
        check(moved);
    }
    {
        auto source = make_source();
        auto target = make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::Vector, 3, 1);
        target = std::move(source);
        check(target);
    }
}

template <typename ValueType>
void test_ensure_shared_storage()
{
    using namespace lagrange;

    // Internal promotion adopts values and initialized capacity.
    {
        auto attr = make_attr<ValueType>(AttributeElement::Vertex, AttributeUsage::Vector, 3, 2);
        attr.set_default_value(ValueType(17));
        attr.reserve_entries(10);
        const auto* old_data = attr.get_all().data();
        attr.set_growth_policy(AttributeGrowthPolicy::AllowWithinCapacity);
        attr.set_shrink_policy(AttributeShrinkPolicy::IgnoreIfExternal);
        attr.set_copy_policy(AttributeCopyPolicy::KeepExternalPtr);
        attr.set_write_policy(AttributeWritePolicy::SilentCopy);
        attr.set_cast_policy(AttributeCastPolicy::DoNotRemapInvalid);
        LA_REQUIRE_THROWS(attr.get_owner());
        REQUIRE(!attr.is_external());
        REQUIRE(attr.get_all().data() == old_data);

        attr.ensure_shared_storage();
        auto values = attr.ref_owner();
        REQUIRE(attr.is_external());
        REQUIRE(attr.is_managed());
        REQUIRE(!attr.is_read_only());
        REQUIRE(attr.get_num_elements() == 2);
        REQUIRE(values.size() == 6);
        REQUIRE(values.ref().data() == old_data);
        for (size_t i = 0; i < values.size(); ++i) {
            REQUIRE(values.get()[i] == ValueType(i));
        }
        // This entry was reserved, but not a live element of the original internal vector.
        REQUIRE(values.get().data()[9] == ValueType(17));
        REQUIRE(attr.get_growth_policy() == AttributeGrowthPolicy::AllowWithinCapacity);
        REQUIRE(attr.get_shrink_policy() == AttributeShrinkPolicy::IgnoreIfExternal);
        REQUIRE(attr.get_copy_policy() == AttributeCopyPolicy::KeepExternalPtr);
        REQUIRE(attr.get_write_policy() == AttributeWritePolicy::SilentCopy);
        REQUIRE(attr.get_cast_policy() == AttributeCastPolicy::DoNotRemapInvalid);

        values.ref()[0] = ValueType(42);
        REQUIRE(attr.get(0, 0) == ValueType(42));
        attr.ref(1, 0) = ValueType(23);
        REQUIRE(values.get()[3] == ValueType(23));
        attr.ensure_shared_storage();
        const auto again = attr.get_owner();
        REQUIRE(again.get().data() == values.get().data());
        REQUIRE(!again.owner().owner_before(values.owner()));
        REQUIRE(!values.owner().owner_before(again.owner()));

        attr.shrink_to_fit();
        REQUIRE(attr.get_owner().get().data() == values.get().data());
        attr.resize_elements(3);
        REQUIRE(attr.get_owner().get().data() == values.get().data());
        REQUIRE(attr.get_owner().size() == 9);
        REQUIRE(attr.get(2, 2) == ValueType(17));
        REQUIRE(values.size() == 6);
        auto copy = Attribute<ValueType>(attr);
        copy.ref(0, 0) = ValueType(31);
        REQUIRE(attr.get(0, 0) == ValueType(31));
    }

    // Casting a promoted attribute must transform only its live values, never the padding that
    // promotion materialized to preserve reserved capacity.
    if constexpr (!std::is_same_v<ValueType, double>) {
        auto attr = make_attr<ValueType>(AttributeElement::Value, AttributeUsage::Scalar, 1, 4);
        attr.set_default_value(ValueType(7));
        attr.reserve_entries(8);
        attr.ensure_shared_storage();
        REQUIRE(attr.get_num_elements() == 4);
        REQUIRE(attr.get_owner().size() == 4);

        auto casted = Attribute<double>::template cast_copy<ValueType>(attr);
        REQUIRE(casted.get_num_elements() == 4);
        REQUIRE(casted.get_all().size() == 4);
        for (size_t i = 0; i < 4; ++i) {
            REQUIRE(casted.get(i, 0) == static_cast<double>(attr.get(i, 0)));
        }
    }

    // Internal promotion normalizes error policies; policies set afterward apply normally.
    for (bool shrink : {false, true}) {
        auto attr = make_attr<ValueType>(AttributeElement::Value, AttributeUsage::Scalar, 1, 4);
        attr.reserve_entries(8);
        attr.ensure_shared_storage();
        auto values = attr.get_owner();
        REQUIRE(attr.get_growth_policy() == AttributeGrowthPolicy::SilentCopy);
        REQUIRE(attr.get_shrink_policy() == AttributeShrinkPolicy::SilentCopy);
        REQUIRE(values.size() == 4);

        attr.set_growth_policy(AttributeGrowthPolicy::ErrorIfExternal);
        attr.set_shrink_policy(AttributeShrinkPolicy::ErrorIfExternal);
        attr.ensure_shared_storage();
        REQUIRE(attr.get_growth_policy() == AttributeGrowthPolicy::ErrorIfExternal);
        REQUIRE(attr.get_shrink_policy() == AttributeShrinkPolicy::ErrorIfExternal);
        LA_REQUIRE_THROWS(attr.resize_elements(5));
        LA_REQUIRE_THROWS(attr.shrink_to_fit());
        REQUIRE(attr.get_num_elements() == 4);
        REQUIRE(attr.get_all().data() == values.get().data());

        attr.set_growth_policy(AttributeGrowthPolicy::SilentCopy);
        attr.set_shrink_policy(AttributeShrinkPolicy::SilentCopy);
        if (shrink) {
            attr.shrink_to_fit();
        } else {
            attr.resize_elements(5);
        }
        REQUIRE(!attr.is_external());
        REQUIRE(attr.get_num_elements() == (shrink ? 4 : 5));
        REQUIRE(attr.get_all().data() != values.get().data());
        for (size_t i = 0; i < values.size(); ++i) {
            REQUIRE(values.get()[i] == ValueType(i));
        }
    }

    // A caller-supplied buffer is genuinely foreign: growth, shrink and copy policies apply.
    {
        auto buffer = std::make_shared<std::vector<ValueType>>(8, ValueType(7));
        Attribute<ValueType> attr(AttributeElement::Value, AttributeUsage::Scalar, 1);
        attr.wrap(make_shared_span(buffer, buffer->data(), buffer->size()), 4);
        LA_REQUIRE_THROWS(attr.resize_elements(5));
        LA_REQUIRE_THROWS(attr.shrink_to_fit());
        auto copy = Attribute<ValueType>(attr);
        REQUIRE(!copy.is_external());
        copy.ref(0) = ValueType(42);
        REQUIRE(attr.get(0) == ValueType(7));
        attr.set_copy_policy(AttributeCopyPolicy::ErrorIfExternal);
        LA_REQUIRE_THROWS(Attribute<ValueType>(attr));
    }

    // Managed external buffers are preserved, including read-only storage.
    {
        auto buffer = std::make_shared<std::vector<ValueType>>(8, ValueType(7));
        auto shared = make_shared_span(buffer, buffer->data(), buffer->size());
        Attribute<ValueType> attr(AttributeElement::Value, AttributeUsage::Scalar, 1);
        attr.wrap(shared, 4);
        attr.ensure_shared_storage();
        REQUIRE(attr.get_growth_policy() == AttributeGrowthPolicy::ErrorIfExternal);
        REQUIRE(attr.get_shrink_policy() == AttributeShrinkPolicy::ErrorIfExternal);
        LA_REQUIRE_THROWS(attr.resize_elements(5));
        LA_REQUIRE_THROWS(attr.shrink_to_fit());
        auto values = attr.ref_owner();
        REQUIRE(values.ref().data() == buffer->data());
        REQUIRE(values.size() == 4);
        REQUIRE(!values.owner().owner_before(shared.owner()));
        REQUIRE(!shared.owner().owner_before(values.owner()));

        attr.wrap_const(SharedSpan<const ValueType>(shared.owner(), shared.size()), 4);
        attr.ensure_shared_storage();
        REQUIRE(attr.get_growth_policy() == AttributeGrowthPolicy::ErrorIfExternal);
        REQUIRE(attr.get_shrink_policy() == AttributeShrinkPolicy::ErrorIfExternal);
        LA_REQUIRE_THROWS(attr.resize_elements(5));
        LA_REQUIRE_THROWS(attr.shrink_to_fit());
        REQUIRE(attr.is_read_only());
        REQUIRE(attr.get_owner().get().data() == buffer->data());
        LA_REQUIRE_THROWS(attr.ref_owner());
        REQUIRE(attr.is_read_only());
        REQUIRE(attr.get_owner().get().data() == buffer->data());

        for (auto policy : {AttributeWritePolicy::SilentCopy, AttributeWritePolicy::WarnAndCopy}) {
            attr.wrap_const(SharedSpan<const ValueType>(shared.owner(), shared.size()), 4);
            attr.set_default_value(ValueType(23));
            attr.set_write_policy(policy);
            auto writable = attr.ref_owner();
            REQUIRE(attr.is_external());
            REQUIRE(attr.is_managed());
            REQUIRE(!attr.is_read_only());
            REQUIRE(writable.ref().data() != buffer->data());
            REQUIRE(writable.size() == 4);
            for (size_t i = 0; i < writable.size(); ++i) {
                REQUIRE(writable.get()[i] == ValueType(7));
            }
            // Retain initialized padding without including it in the exported extent.
            for (size_t i = writable.size(); i < shared.size(); ++i) {
                REQUIRE(writable.get().data()[i] == ValueType(23));
            }
            writable.ref()[0] = ValueType(42);
            REQUIRE(attr.get(0) == ValueType(42));
            REQUIRE((*buffer)[0] == ValueType(7));
        }
    }

    // Empty promotion and null alias owners retain lifetime.
    {
        Attribute<ValueType> attr(AttributeElement::Value, AttributeUsage::Scalar, 1);
        attr.ensure_shared_storage();
        REQUIRE(attr.is_external());
        REQUIRE(attr.is_managed());
        REQUIRE(attr.get_owner().has_owner());
        REQUIRE(attr.ref_owner().size() == 0);

        auto buffer = std::make_shared<std::vector<ValueType>>();
        attr.set_growth_policy(AttributeGrowthPolicy::ErrorIfExternal);
        attr.set_shrink_policy(AttributeShrinkPolicy::ErrorIfExternal);
        auto shared = make_shared_span(buffer, static_cast<ValueType*>(nullptr), 0);
        attr.wrap(shared, 0);
        attr.ensure_shared_storage();
        REQUIRE(attr.get_growth_policy() == AttributeGrowthPolicy::ErrorIfExternal);
        REQUIRE(attr.get_shrink_policy() == AttributeShrinkPolicy::ErrorIfExternal);
        LA_REQUIRE_THROWS(attr.resize_elements(1));
        auto empty = attr.get_owner();
        REQUIRE(attr.is_managed());
        REQUIRE(empty.has_owner());
        REQUIRE(empty.get().data() == nullptr);
        REQUIRE(!empty.owner().owner_before(shared.owner()));
        REQUIRE(!shared.owner().owner_before(empty.owner()));
        REQUIRE(attr.ref_owner().has_owner());

        attr.wrap_const(SharedSpan<const ValueType>(shared.owner(), shared.size()), 0);
        attr.ensure_shared_storage();
        REQUIRE(attr.is_managed());
        REQUIRE(attr.is_read_only());
        LA_REQUIRE_THROWS(attr.ref_owner());
        REQUIRE(attr.get_owner().has_owner());

        attr.wrap(SharedSpan<ValueType>(), 0);
        REQUIRE(!attr.is_managed());
        LA_REQUIRE_THROWS(attr.ensure_shared_storage());
        LA_REQUIRE_THROWS(attr.get_owner());
        attr.wrap_const(SharedSpan<const ValueType>(), 0);
        REQUIRE(!attr.is_managed());
        REQUIRE(attr.is_read_only());
        LA_REQUIRE_THROWS(attr.ensure_shared_storage());
    }

    // Unmanaged storage is rejected without changing borrowed values.
    {
        std::vector<ValueType> buffer(4, ValueType(7));
        Attribute<ValueType> attr(AttributeElement::Value, AttributeUsage::Scalar, 1);
        attr.wrap(buffer, 4);
        LA_REQUIRE_THROWS(attr.ensure_shared_storage());
        LA_REQUIRE_THROWS(attr.get_owner());
        LA_REQUIRE_THROWS(attr.ref_owner());
        REQUIRE(!attr.is_managed());
        REQUIRE(attr.ref_all().data() == buffer.data());
        attr.ref(0) = ValueType(42);
        REQUIRE(buffer[0] == ValueType(42));

        attr.wrap_const(buffer, 4);
        LA_REQUIRE_THROWS(attr.ensure_shared_storage());
        LA_REQUIRE_THROWS(attr.ref_owner());
        REQUIRE(attr.is_read_only());
        REQUIRE(attr.get_all().data() == buffer.data());
        // A deliberate write-policy copy may replace borrowed read-only storage with owned storage.
        attr.set_write_policy(AttributeWritePolicy::SilentCopy);
        auto values = attr.ref_owner();
        values.ref()[0] = ValueType(23);
        REQUIRE(attr.is_managed());
        REQUIRE(attr.get(0) == ValueType(23));
        REQUIRE(buffer[0] == ValueType(42));
    }
}

template <typename ValueType>
void test_managed_lifetime()
{
    using namespace lagrange;
    SharedSpan<ValueType> retained;
    std::weak_ptr<const ValueType> lifetime;
    {
        auto attr = make_attr<ValueType>(AttributeElement::Value, AttributeUsage::Scalar, 1, 4);
        retained = attr.ref_owner();
        lifetime = retained.owner();
        Attribute<ValueType> moved(std::move(attr));
        REQUIRE(moved.get_owner().get().data() == retained.get().data());
        moved.ref(1) = ValueType(42);
        REQUIRE(retained.get()[1] == ValueType(42));

        moved.create_internal_copy();
        REQUIRE(!moved.is_external());
        moved.ref(1) = ValueType(23);
        REQUIRE(retained.get()[1] == ValueType(42));
        REQUIRE(!lifetime.expired());
        moved.ensure_shared_storage();
        REQUIRE(moved.get_owner().get().data() != retained.get().data());
        moved.wrap(retained.subspan(1, 3), 3);
        REQUIRE(moved.get(0) == ValueType(42));
        moved.wrap(retained, 4);
        REQUIRE(moved.get(1) == ValueType(42));
        auto replacement = std::make_shared<std::vector<ValueType>>(4, ValueType(9));
        moved.wrap(make_shared_span(replacement, replacement->data(), replacement->size()), 4);
        REQUIRE(moved.get(1) == ValueType(9));
        REQUIRE(retained.get()[1] == ValueType(42));
    }
    REQUIRE(!lifetime.expired());
    REQUIRE(retained.get()[0] == ValueType(0));
    REQUIRE(retained.get()[1] == ValueType(42));
    REQUIRE(retained.get()[3] == ValueType(3));
    retained = {};
    REQUIRE(lifetime.expired());

    // Destruction of the attribute actively using this allocation must not destroy the buffer.
    {
        auto attr = make_attr<ValueType>(AttributeElement::Value, AttributeUsage::Scalar, 1, 2);
        retained = attr.ref_owner();
        lifetime = retained.owner();
        Attribute<ValueType> destination(AttributeElement::Value, AttributeUsage::Scalar, 1);
        destination = std::move(attr);
        destination.ref(1) = ValueType(23);
        REQUIRE(retained.get()[1] == ValueType(23));
    }
    REQUIRE(!lifetime.expired());
    REQUIRE(retained.get()[1] == ValueType(23));
    retained = {};
    REQUIRE(lifetime.expired());
}

template <typename ValueType>
void test_managed_detachment()
{
    using namespace lagrange;
    auto attr = make_attr<ValueType>(AttributeElement::Value, AttributeUsage::Scalar, 1, 4);
    attr.reserve_entries(9);
    attr.set_default_value(ValueType(17));
    attr.set_growth_policy(AttributeGrowthPolicy::SilentCopy);
    attr.set_shrink_policy(AttributeShrinkPolicy::SilentCopy);
    auto original = attr.ref_owner();
    original.ref()[0] = ValueType(42);
    attr.resize_elements(2);
    REQUIRE(!attr.is_external());
    REQUIRE(attr.get_num_elements() == 2);
    REQUIRE(attr.get(0) == ValueType(42));
    REQUIRE(original.size() == 4);
    REQUIRE(original.get()[3] == ValueType(3));
    REQUIRE(original.get().data()[8] == ValueType(17));
    attr.ref(0) = ValueType(23);
    REQUIRE(original.get()[0] == ValueType(42));

    auto shrunk = attr.ref_owner();
    REQUIRE(shrunk.size() == 2);
    REQUIRE(shrunk.get().data() != original.get().data());
    attr.shrink_to_fit();
    REQUIRE(!attr.is_external());
    REQUIRE(shrunk.get()[0] == ValueType(23));
    REQUIRE(shrunk.get().data()[8] == ValueType(17));
    attr.ref(0) = ValueType(31);
    REQUIRE(shrunk.get()[0] == ValueType(23));

    auto grown_from = attr.ref_owner();
    attr.resize_elements(5);
    REQUIRE(!attr.is_external());
    REQUIRE(attr.get(0) == ValueType(31));
    REQUIRE(attr.get(4) == ValueType(17));
    REQUIRE(grown_from.size() == 2);
    REQUIRE(grown_from.get()[0] == ValueType(31));
    auto grown = attr.ref_owner();
    REQUIRE(grown.size() == 5);
    REQUIRE(grown.get().data() != grown_from.get().data());
    grown.ref()[4] = ValueType(42);
    REQUIRE(attr.get(4) == ValueType(42));
}

template <typename ValueType>
void test_shared_span()
{
    using namespace lagrange;
    auto buffer = std::make_shared<std::vector<ValueType>>(4);
    std::iota(buffer->begin(), buffer->end(), ValueType(0));
    auto values = make_shared_span(buffer, buffer->data(), buffer->size());
    std::weak_ptr<std::vector<ValueType>> lifetime = buffer;
    auto middle = values.subspan(1, 2);
    SharedSpan<const ValueType> readonly(middle.owner(), middle.size());
    auto end = values.subspan(4, 0);
    REQUIRE(middle.ref().data() == buffer->data() + 1);
    REQUIRE(middle.size() == 2);
    REQUIRE(end.has_owner());
    REQUIRE(end.size() == 0);
    REQUIRE(end.get().data() == buffer->data() + 4);
    middle.ref()[0] = ValueType(42);
    REQUIRE(readonly.get()[0] == ValueType(42));
    LA_REQUIRE_THROWS(values.subspan(5, 0));
    LA_REQUIRE_THROWS(values.subspan(3, 2));
    LA_REQUIRE_THROWS(values.subspan(1, std::numeric_limits<size_t>::max()));
    buffer.reset();
    values = {};
    middle = {};
    readonly = {};
    REQUIRE(!lifetime.expired());
    end = {};
    REQUIRE(lifetime.expired());

    auto owner = std::make_shared<std::vector<ValueType>>();
    auto empty = make_shared_span(owner, static_cast<ValueType*>(nullptr), 0);
    std::weak_ptr<std::vector<ValueType>> empty_lifetime = owner;
    auto empty_end = empty.subspan(0, 0);
    SharedSpan<const ValueType> const_empty(empty_end.owner(), empty_end.size());
    REQUIRE(empty.has_owner());
    REQUIRE(empty_end.get().data() == nullptr);
    REQUIRE(const_empty.has_owner());
    LA_REQUIRE_THROWS(empty.subspan(1, 0));
    LA_REQUIRE_THROWS(empty.subspan(0, 1));
    owner.reset();
    empty = {};
    empty_end = {};
    REQUIRE(!empty_lifetime.expired());
    const_empty = {};
    REQUIRE(empty_lifetime.expired());
    SharedSpan<ValueType> unowned;
    REQUIRE(!unowned.has_owner());
    REQUIRE(!unowned.subspan(0, 0).has_owner());
}

} // namespace

TEST_CASE("IndexedAttribute: Move-Copy", "[attribute]")
{
#define LA_X_move_copy_indexed_values(I, S) test_move_copy_indexed<S, I>();
#define LA_X_move_copy_indexed_indices(_, I) LA_ATTRIBUTE_X(move_copy_indexed_values, I)
    LA_SURFACE_MESH_INDEX_X(move_copy_indexed_indices, 0)
}

TEST_CASE("Attribute: Create", "[attribute]")
{
#define LA_X_create_attribute(_, ValueType) test_create_attribute<ValueType>();
    LA_ATTRIBUTE_X(create_attribute, 0)
}

TEST_CASE("Attribute: Move-Copy Internal", "[attribute]")
{
#define LA_X_move_copy_internal(_, ValueType) test_move_copy_internal<ValueType>();
    LA_ATTRIBUTE_X(move_copy_internal, 0)
}

TEST_CASE("Attribute: Move-Copy External", "[attribute]")
{
#define LA_X_move_copy_external(_, ValueType) test_move_copy_external<ValueType>();
    LA_ATTRIBUTE_X(move_copy_external, 0)
}

TEST_CASE("Attribute: Data Access", "[attribute]")
{
#define LA_X_data_access(_, ValueType) test_data_access<ValueType>();
    LA_ATTRIBUTE_X(data_access, 0)
}

TEST_CASE("Attribute: Write Policy", "[attribute]")
{
#define LA_X_write_policy(_, ValueType) test_write_policy<ValueType>();
    LA_ATTRIBUTE_X(write_policy, 0)
}

TEST_CASE("Attribute: Growth Policy", "[attribute]")
{
#define LA_X_growth_policy(_, ValueType) test_growth_policy<ValueType>();
    LA_ATTRIBUTE_X(growth_policy, 0)
}

TEST_CASE("Attribute: Empty Buffers", "[attribute]")
{
#define LA_X_empty_buffers(_, ValueType) test_empty_buffers<ValueType>();
    LA_ATTRIBUTE_X(empty_buffers, 0)
}

TEST_CASE("Attribute: ownership", "[attribute]")
{
#define LA_X_ownership(_, ValueType) test_ownership<ValueType>();
    LA_ATTRIBUTE_X(ownership, 0)
}

TEST_CASE("Attribute: Policy propagation", "[attribute]")
{
#define LA_X_policy_propagation(_, ValueType) test_policy_propagation<ValueType>();
    LA_ATTRIBUTE_X(policy_propagation, 0)
}

TEST_CASE("Attribute: Managed promotion", "[attribute]")
{
#define LA_X_ensure_shared_storage(_, ValueType) test_ensure_shared_storage<ValueType>();
    LA_ATTRIBUTE_X(ensure_shared_storage, 0)
}

TEST_CASE("Attribute: Managed allocation lifetime", "[attribute]")
{
#define LA_X_managed_lifetime(_, ValueType) test_managed_lifetime<ValueType>();
    LA_ATTRIBUTE_X(managed_lifetime, 0)
}

TEST_CASE("Attribute: Managed allocation detachment", "[attribute]")
{
#define LA_X_managed_detachment(_, ValueType) test_managed_detachment<ValueType>();
    LA_ATTRIBUTE_X(managed_detachment, 0)
}

TEST_CASE("SharedSpan: Ownership and bounded views", "[utils][attribute]")
{
#define LA_X_shared_span(_, ValueType) test_shared_span<ValueType>();
    LA_ATTRIBUTE_X(shared_span, 0)
}

// TODO
// - Test reserve entries with external buffer (const and non-const)
// - Test resize elements with external buffer (decrease, equal, and increase capacity) for all
//   policies
// - Test insert elements with external buffer for all policies
// - Test default value + attribute growth

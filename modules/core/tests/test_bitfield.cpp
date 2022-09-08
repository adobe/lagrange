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
#include <lagrange/AttributeFwd.h>
#include <lagrange/utils/BitField.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <catch2/catch_test_macros.hpp>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace {

using namespace lagrange;
using AttributeElementField = BitField<AttributeElement>;
using UnderlyingType = AttributeElementField::UnderlyingType;

auto zero_bit = static_cast<UnderlyingType>(0);
auto vertex_bit = static_cast<UnderlyingType>(AttributeElement::Vertex);
auto facet_bit = static_cast<UnderlyingType>(AttributeElement::Facet);
auto edge_bit = static_cast<UnderlyingType>(AttributeElement::Edge);
auto corner_bit = static_cast<UnderlyingType>(AttributeElement::Corner);
auto indexed_bit = static_cast<UnderlyingType>(AttributeElement::Indexed);
auto value_bit = static_cast<UnderlyingType>(AttributeElement::Value);

} // namespace

TEST_CASE("BitField: zero", "[next]")
{
    AttributeElementField bitfield;
    REQUIRE(bitfield.get_value() == zero_bit);
    REQUIRE(int(bitfield) == zero_bit);
    REQUIRE(bitfield == AttributeElementField::none());
}

TEST_CASE("BitField: all", "[next]")
{
    AttributeElementField bitfield = AttributeElementField::all();
    REQUIRE(bitfield.get_value() == ~zero_bit);
}

TEST_CASE("BitField: with value", "[next]")
{
    AttributeElementField bitfield(AttributeElement::Vertex);
    REQUIRE(bitfield.get_value() == vertex_bit);
}

TEST_CASE("BitField: set bit", "[next]")
{
    AttributeElementField bitfield(AttributeElement::Vertex);
    REQUIRE(bitfield.get_value() == vertex_bit);
    bitfield.set(AttributeElement::Vertex);
    REQUIRE(bitfield.get_value() == vertex_bit);
    bitfield.set(AttributeElement::Facet);
    REQUIRE(bitfield.get_value() == (facet_bit | vertex_bit));
}

TEST_CASE("BitField: test bit", "[next]")
{
    AttributeElementField bitfield;
    REQUIRE(!bitfield.test(AttributeElement::Vertex));
    REQUIRE(!bitfield.test(AttributeElement::Facet));
    REQUIRE(!bitfield.test(AttributeElement::Edge));
    REQUIRE(!bitfield.test(AttributeElement::Corner));
    REQUIRE(!bitfield.test(AttributeElement::Indexed));
    REQUIRE(!bitfield.test(AttributeElement::Value));

    bitfield.set(AttributeElement::Vertex);
    REQUIRE(bitfield.test(AttributeElement::Vertex));
    bitfield.set(AttributeElement::Facet);
    REQUIRE(bitfield.test(AttributeElement::Facet));
    bitfield.set(AttributeElement::Edge);
    REQUIRE(bitfield.test(AttributeElement::Edge));
    bitfield.set(AttributeElement::Corner);
    REQUIRE(bitfield.test(AttributeElement::Corner));
    REQUIRE(bitfield.test(vertex_bit | facet_bit | edge_bit | corner_bit));
}

TEST_CASE("BitField: test any bit", "[next]")
{
    AttributeElementField bitfield(AttributeElement::Facet | AttributeElement::Vertex);
    REQUIRE(bitfield.test_any(facet_bit));
    REQUIRE(bitfield.test_any(facet_bit | corner_bit));
    REQUIRE(!bitfield.test_any(edge_bit | corner_bit));
    REQUIRE(!bitfield.test_any(edge_bit | value_bit | indexed_bit));
}

TEST_CASE("BitField: clear bit", "[next]")
{
    AttributeElementField bitfield(AttributeElement::Facet | AttributeElement::Vertex);
    REQUIRE(bitfield.test(vertex_bit));
    REQUIRE(bitfield.test(facet_bit));
    bitfield.clear(facet_bit);
    REQUIRE(bitfield.test(vertex_bit));
    REQUIRE(!bitfield.test(facet_bit));
}

TEST_CASE("BitField: clear all", "[next]")
{
    AttributeElementField bitfield(
        AttributeElement::Facet | AttributeElement::Vertex | AttributeElement::Edge);
    REQUIRE(bitfield.test(vertex_bit | facet_bit | edge_bit));
    bitfield.clear_all();
    REQUIRE(!bitfield.test(vertex_bit | facet_bit | edge_bit));
    REQUIRE(bitfield.test(zero_bit));
}

TEST_CASE("BitField: operator not", "[next]")
{
    AttributeElementField bitfield;
    UnderlyingType all_bits = static_cast<UnderlyingType>(0);
    all_bits = ~all_bits;
    REQUIRE(!bitfield.test(all_bits));
    bitfield = ~bitfield;
    REQUIRE(bitfield.test(all_bits));
}

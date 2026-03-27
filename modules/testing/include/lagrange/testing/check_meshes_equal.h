/*
 * Copyright 2026 Adobe. All rights reserved.
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

#include <lagrange/Attribute.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/foreach_attribute.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <catch2/catch_test_macros.hpp>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <string>
#include <vector>

namespace lagrange::testing {

///
/// Check that two meshes are bitwise identical.
///
/// Verifies topology counts, all attribute names, attribute IDs for user attributes,
/// and bitwise equality of all attribute data (both reserved and non-reserved).
/// This is suitable for verifying lossless serialization round-trips.
///
/// @param[in]  a  First mesh.
/// @param[in]  b  Second mesh.
///
/// @tparam Scalar  Mesh scalar type.
/// @tparam Index   Mesh index type.
///
template <typename Scalar, typename Index>
void check_meshes_equal(const SurfaceMesh<Scalar, Index>& a, const SurfaceMesh<Scalar, Index>& b)
{
    // Check topology counts
    REQUIRE(a.get_num_vertices() == b.get_num_vertices());
    REQUIRE(a.get_num_facets() == b.get_num_facets());
    REQUIRE(a.get_num_corners() == b.get_num_corners());
    REQUIRE(a.get_num_edges() == b.get_num_edges());
    REQUIRE(a.get_dimension() == b.get_dimension());

    if (a.is_regular()) {
        REQUIRE(b.is_regular());
        REQUIRE(a.get_vertex_per_facet() == b.get_vertex_per_facet());
    } else {
        REQUIRE(b.is_hybrid());
    }

    // Collect all attribute names from both meshes
    std::vector<std::pair<std::string, AttributeId>> names_a, names_b;
    a.seq_foreach_attribute_id([&](std::string_view name, AttributeId id) {
        names_a.emplace_back(std::string(name), id);
    });
    b.seq_foreach_attribute_id([&](std::string_view name, AttributeId id) {
        names_b.emplace_back(std::string(name), id);
    });

    // Check that all attributes in a exist in b
    for (const auto& [name, id] : names_a) {
        INFO("Attribute in a missing from b: " << name);
        REQUIRE(b.has_attribute(name));
    }

    // Check that all attributes in b exist in a
    for (const auto& [name, id] : names_b) {
        INFO("Attribute in b missing from a: " << name);
        REQUIRE(a.has_attribute(name));
    }

    // Check that user attribute IDs match
    for (const auto& [name, id_a] : names_a) {
        if (SurfaceMesh<Scalar, Index>::attr_name_is_reserved(name)) continue;
        INFO("Checking attribute id for: " << name);
        REQUIRE(b.get_attribute_id(name) == id_a);
    }

    // Check all attribute data (reserved and non-reserved)
    seq_foreach_named_attribute_read(a, [&](std::string_view name, auto&& attr_a) {
        using AttrType = std::decay_t<decltype(attr_a)>;

        INFO("Checking attribute data: " << std::string(name));
        REQUIRE(b.has_attribute(name));

        if constexpr (AttrType::IsIndexed) {
            using ValueType = typename AttrType::ValueType;
            const auto& attr_b = b.template get_indexed_attribute<ValueType>(name);
            auto vals_a = attr_a.values().get_all();
            auto vals_b = attr_b.values().get_all();
            REQUIRE(vals_a.size() == vals_b.size());
            for (size_t i = 0; i < vals_a.size(); ++i) {
                REQUIRE(vals_a[i] == vals_b[i]);
            }
            auto idx_a = attr_a.indices().get_all();
            auto idx_b = attr_b.indices().get_all();
            REQUIRE(idx_a.size() == idx_b.size());
            for (size_t i = 0; i < idx_a.size(); ++i) {
                REQUIRE(idx_a[i] == idx_b[i]);
            }
        } else {
            using ValueType = typename AttrType::ValueType;
            const auto& attr_b = b.template get_attribute<ValueType>(name);
            REQUIRE(attr_a.get_num_elements() == attr_b.get_num_elements());
            REQUIRE(attr_a.get_num_channels() == attr_b.get_num_channels());
            auto span_a = attr_a.get_all();
            auto span_b = attr_b.get_all();
            REQUIRE(span_a.size() == span_b.size());
            for (size_t i = 0; i < span_a.size(); ++i) {
                REQUIRE(span_a[i] == span_b[i]);
            }
        }
    });
}

} // namespace lagrange::testing

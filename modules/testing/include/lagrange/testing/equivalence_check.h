/*
 * Copyright 2023 Adobe. All rights reserved.
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
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/map_attribute.h>
#include <lagrange/views.h>

#include <catch2/catch_test_macros.hpp>

namespace lagrange::testing {

/**
 * Check if two attributes from two different meshes are equivalent.
 *
 * Note that the two input attribute may have different `AttributeElement`. By being equivalent, it
 * means the two attributes has the same vale at each corner when remapped to corner attributes.
 *
 * @tparam ValueType1 The value type of attribute 1.
 * @tparam ValueType2 The value type of attribute 2.
 * @tparam Scalar     The scalar type.
 * @tparam Index      The index type.
 *
 * @param mesh1    The first input mesh.
 * @param mesh2    The second input mesh.
 * @param id1      The id of the attribute in `mesh1`.
 * @param id2      The id of the attribute in `mesh2`.
 *
 * @return True if the two attributes are equivalent.
 */
template <typename ValueType1, typename ValueType2, typename Scalar, typename Index>
bool attribute_is_approx_equivalent(
    SurfaceMesh<Scalar, Index>& mesh1,
    SurfaceMesh<Scalar, Index>& mesh2,
    AttributeId id1,
    AttributeId id2)
{
    std::string_view name = "testing_attribute_is_approx_equivalent";
    id1 = map_attribute(mesh1, id1, name, AttributeElement::Corner);
    id2 = map_attribute(mesh2, id2, name, AttributeElement::Corner);

    const auto& attr1 = mesh1.template get_attribute<ValueType1>(id1);
    const auto& attr2 = mesh2.template get_attribute<ValueType2>(id2);

    REQUIRE(attr1.get_usage() == attr2.get_usage());
    REQUIRE(attr1.get_num_channels() == attr2.get_num_channels());
    REQUIRE(attr1.get_num_elements() == attr2.get_num_elements());

    auto data1 = matrix_view(attr1);
    auto data2 = matrix_view(attr2);

    constexpr Scalar rel_tol = static_cast<Scalar>(1e-6);
    bool match = data1.template cast<Scalar>().isApprox(data2.template cast<Scalar>(), rel_tol);

    mesh1.delete_attribute(name);
    mesh2.delete_attribute(name);
    return match;
}

/**
 * Check if two meshes have equivalent attributes for the specified usage.
 *
 * For special usage attributes, we do not rely on attribute names.  Instead, we check for all
 * possible attriabute pairs with matching usage type for equivalance. Two attributes are equivalent
 * if they are the same (upto casting) when they are both mapped to corner attribute.
 *
 * @tparam usage   The target usage of the attributes.
 * @tparam Scalar  The scalar type.
 * @tparam Index   The index type.
 *
 * @param mesh1    Input mesh 1.
 * @param mesh2    Input mesh 2.
 */
template <AttributeUsage usage, typename Scalar, typename Index>
void ensure_approx_equivalent_usage(
    SurfaceMesh<Scalar, Index>& mesh1,
    SurfaceMesh<Scalar, Index>& mesh2)
{
    seq_foreach_named_attribute_read(mesh1, [&](std::string_view name1, auto&& attr1) {
        using ValueType1 = typename std::decay_t<decltype(attr1)>::ValueType;
        if (attr1.get_usage() != usage) return;
        auto id1 = mesh1.get_attribute_id(name1);

        bool has_equivalent = false;
        seq_foreach_named_attribute_read(mesh2, [&](std::string_view name2, auto&& attr2) {
            using ValueType2 = typename std::decay_t<decltype(attr2)>::ValueType;
            if (attr2.get_usage() != usage) return;
            auto id2 = mesh2.get_attribute_id(name2);
            if (attribute_is_approx_equivalent<ValueType1, ValueType2>(mesh1, mesh2, id1, id2))
                has_equivalent = true;
        });

        REQUIRE(has_equivalent);
    });
}

/**
 * Ensure two meshes are equivalent.
 *
 * Note that two meshes are equivalent if they have equivalent set of attributes.  The following
 * attributes are checked:
 *
 *   * Positions.
 *   * Special usage attributes like uv, normal, color, etc.
 *   * Vertex/Facet/Corner/Indexed attribute with Scalar or Vector usage.
 *
 * Two attributes are equivalent if they are the same (upto casting) when they are both mapped to
 * corner attribute.
 *
 * @tparam Scalar  The scalar type.
 * @tparam Index   The index type.
 *
 * @param mesh1    The first input mesh.
 * @param mesh2    The second input mesh.
 */
template <typename Scalar, typename Index>
void ensure_approx_equivalent_mesh(
    SurfaceMesh<Scalar, Index>& mesh1,
    SurfaceMesh<Scalar, Index>& mesh2)
{
    // Ensure vertices are equivalent.
    REQUIRE(testing::attribute_is_approx_equivalent<Scalar, Scalar>(
        mesh1,
        mesh2,
        mesh1.attr_id_vertex_to_positions(),
        mesh2.attr_id_vertex_to_positions()));

    // Special attributes are compared based on usage.
    testing::ensure_approx_equivalent_usage<AttributeUsage::Normal>(mesh1, mesh2);
    testing::ensure_approx_equivalent_usage<AttributeUsage::UV>(mesh1, mesh2);
    testing::ensure_approx_equivalent_usage<AttributeUsage::Color>(mesh1, mesh2);
    testing::ensure_approx_equivalent_usage<AttributeUsage::Tangent>(mesh1, mesh2);
    testing::ensure_approx_equivalent_usage<AttributeUsage::Bitangent>(mesh1, mesh2);

    // For all other attributes, compare with the attribute in the other mesh with the same name.
    seq_foreach_named_attribute_read(mesh1, [&](std::string_view name, auto&& attr1) {
        if (mesh1.attr_name_is_reserved(name)) return;
        auto usage = attr1.get_usage();
        if (usage != AttributeUsage::Scalar && usage != AttributeUsage::Vector) return;
        auto element = attr1.get_element_type();

        // We do not check edge or value attributes as there is no clear mapping of them across
        // multiple meshes.
        if (element == AttributeElement::Edge || element == AttributeElement::Value) return;

        using ValueType1 = typename std::decay_t<decltype(attr1)>::ValueType;
        AttributeId id1 = mesh1.get_attribute_id(name);
        AttributeId id2 = mesh2.get_attribute_id(name);

        details::internal_foreach_named_attribute<
            BitField<AttributeElement>::all(),
            details::Ordering::Sequential,
            details::Access::Read>(
            mesh2,
            [&](std::string_view name2, auto&& attr2) {
                if (mesh2.attr_name_is_reserved(name2)) return;

                using ValueType2 = typename std::decay_t<decltype(attr2)>::ValueType;
                REQUIRE(testing::attribute_is_approx_equivalent<ValueType1, ValueType2>(
                    mesh1,
                    mesh2,
                    id1,
                    id2));
            },
            {&id2, 1});
    });
}

} // namespace lagrange::testing

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
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/map_attribute.h>

#include <catch2/catch_approx.hpp>

#include <random>

namespace fs = lagrange::fs;

namespace {

template <typename Scalar>
void assert_same_approx(lagrange::span<const Scalar> l, lagrange::span<const Scalar> r)
{
    REQUIRE(l.size() == r.size());
    for (size_t i = 0; i < l.size(); ++i) {
        REQUIRE(Catch::Approx(l[i]) == r[i]);
    }
}

constexpr bool is_averaging(lagrange::AttributeElement left, lagrange::AttributeElement right)
{
    using namespace lagrange;
    const auto V = AttributeElement::Vertex;
    const auto F = AttributeElement::Facet;
    const auto E = AttributeElement::Edge;
    const auto C = AttributeElement::Corner;
    const auto I = AttributeElement::Indexed;
    auto check = [&](auto a, auto b) {
        return (a == V && b == F) || (a == V && b == E) || (a == F && b == E);
    };
    return check(left, right) || check(right, left) ||
           ((left == C || left == I) && (right == V || right == F || right == E));
}

template <typename ValueType, typename Scalar, typename Index>
void test_map_attribute_one(
    const lagrange::SurfaceMesh<Scalar, Index> original_mesh,
    size_t num_channels)
{
    using namespace lagrange;

    std::mt19937 gen;

    std::vector<AttributeElement> src_elements = {
        AttributeElement::Vertex,
        AttributeElement::Facet,
        AttributeElement::Corner,
        AttributeElement::Indexed,
        AttributeElement::Value};

    std::vector<AttributeElement> dst_elements = {
        AttributeElement::Vertex,
        AttributeElement::Facet,
        AttributeElement::Corner,
        AttributeElement::Indexed,
        AttributeElement::Value};

    if (original_mesh.has_edges()) {
        src_elements.push_back(AttributeElement::Edge);
        dst_elements.push_back(AttributeElement::Edge);
    }

    auto dist_values = [] {
        if constexpr (std::is_integral_v<ValueType>) {
            // Apparently std::uniform_int_distribution<uint8_t>() is not a thing, so use `int`
            return std::uniform_int_distribution<int>(0, 6);
        } else {
            return std::uniform_real_distribution<ValueType>(0, ValueType(6));
        }
    }();

    for (auto src_element : src_elements) {
        for (auto dst_element : dst_elements) {
            lagrange::SurfaceMesh<Scalar, Index> mesh = original_mesh;
            std::string_view name = "foo";
            auto id = mesh.template create_attribute<ValueType>(
                name,
                src_element,
                lagrange::AttributeUsage::Vector,
                num_channels);

            Attribute<ValueType>* values_ptr;
            {
                if (src_element == AttributeElement::Indexed) {
                    values_ptr = &mesh.template ref_indexed_attribute<ValueType>(id).values();
                } else {
                    values_ptr = &mesh.template ref_attribute<ValueType>(id);
                }
            }
            Attribute<ValueType>& values = *values_ptr;

            if (src_element == AttributeElement::Indexed) {
                // Compute a random index buffer
                auto& indices = mesh.template ref_indexed_attribute<ValueType>(id).indices();
                const Index num_indices =
                    std::uniform_int_distribution<Index>(1, mesh.get_num_corners())(gen);
                std::uniform_int_distribution<Index> dist_indices(0, num_indices - 1);
                values.resize_elements(num_indices);
                for (auto& x : indices.ref_all()) {
                    x = dist_indices(gen);
                }
            } else if (src_element == AttributeElement::Value) {
                // Set attribute size based on target element type
                switch (dst_element) {
                case AttributeElement::Vertex:
                    values.resize_elements(mesh.get_num_vertices());
                    break;
                case AttributeElement::Facet: values.resize_elements(mesh.get_num_facets()); break;
                case AttributeElement::Edge: values.resize_elements(mesh.get_num_edges()); break;
                case AttributeElement::Corner:
                case AttributeElement::Indexed:
                    values.resize_elements(mesh.get_num_corners());
                    break;
                case AttributeElement::Value: values.resize_elements(42); break;
                }
            }

            // Populate initial values
            for (auto& x : values.ref_all()) {
                if (is_averaging(src_element, dst_element)) {
                    // If the forward mapping is averaging, we just check that a constant
                    // field is preserved by both mappings.
                    x = ValueType(5);
                } else {
                    // Otherwise, let's initialize our attribute with random values.
                    x = static_cast<ValueType>(dist_values(gen));
                }
            }

            auto new_id = map_attribute(mesh, id, "new_foo", dst_element);
            auto old_id = map_attribute(mesh, new_id, "old_foo", src_element);
            if (src_element != AttributeElement::Indexed) {
                assert_same_approx(
                    mesh.template get_attribute<ValueType>(id).get_all(),
                    mesh.template get_attribute<ValueType>(old_id).get_all());
            } else {
                // Index attributes might have their value buffer deduplicated, but
                // otherwise the content should match.
                const auto& attr = mesh.template get_indexed_attribute<ValueType>(id);
                const auto& old_attr = mesh.template get_indexed_attribute<ValueType>(old_id);
                for (Index c = 0; c < mesh.get_num_corners(); ++c) {
                    const auto& l = attr.values().get_row(attr.indices().get(c));
                    const auto& r = old_attr.values().get_row(old_attr.indices().get(c));
                    REQUIRE(std::equal(l.begin(), l.end(), r.begin()));
                }
            }

            // Test mapping in place
            REQUIRE(mesh.has_attribute(name));
            map_attribute_in_place(mesh, name, dst_element);
            REQUIRE(mesh.has_attribute(name));
        }
    }
}

template <typename ValueType, typename Scalar, typename Index>
void test_map_attribute_value_invalid(
    const lagrange::SurfaceMesh<Scalar, Index> original_mesh,
    size_t num_channels)
{
    using namespace lagrange;

    std::mt19937 gen;

    std::vector<AttributeElement> dst_elements = {
        AttributeElement::Vertex,
        AttributeElement::Facet,
        AttributeElement::Corner,
        AttributeElement::Indexed};

    if (original_mesh.has_edges()) {
        dst_elements.push_back(AttributeElement::Edge);
    }

    const Index num_values_elements = 42;
    for (auto dst_element : dst_elements) {
        lagrange::SurfaceMesh<Scalar, Index> mesh = original_mesh;
        std::string_view name = "foo";
        auto id = mesh.template create_attribute<ValueType>(
            name,
            lagrange::AttributeElement::Value,
            lagrange::AttributeUsage::Vector,
            num_channels);

        Attribute<ValueType>& values = mesh.template ref_attribute<ValueType>(id);

        // Check that we are creating a mismatch with target attribute size
        switch (dst_element) {
        case AttributeElement::Vertex:
            REQUIRE(num_values_elements != mesh.get_num_vertices());
            break;
        case AttributeElement::Facet: REQUIRE(num_values_elements != mesh.get_num_facets()); break;
        case AttributeElement::Edge: REQUIRE(num_values_elements != mesh.get_num_edges()); break;
        case AttributeElement::Corner:
        case AttributeElement::Indexed:
            REQUIRE(num_values_elements != mesh.get_num_corners());
            break;
        default: la_debug_assert(false); break;
        }
        values.resize_elements(num_values_elements);

        LA_REQUIRE_THROWS(map_attribute(mesh, id, "new_foo", dst_element));
    }
}

template <typename Scalar, typename Index>
void test_map_attribute_types(lagrange::SurfaceMesh<Scalar, Index> mesh, size_t num_channels)
{
#define LA_X_map_attribute_one(_, ValueType) test_map_attribute_one<ValueType>(mesh, num_channels);
    LA_ATTRIBUTE_X(map_attribute_one, 0)
}

template <typename Scalar, typename Index>
void test_map_attribute_all()
{
    using MeshType = lagrange::SurfaceMesh<Scalar, Index>;

    auto filenames = {
        "poly/L-plane.obj",
        "poly/mixedFaringPart.obj",
        "poly/tetris.obj",
        "cube_soup.obj"};

    for (fs::path filename : filenames) {
        fs::path input_path = lagrange::testing::get_data_path("open/core" / filename);
        REQUIRE(fs::exists(input_path));

        auto mesh = lagrange::io::load_mesh_obj<MeshType>(input_path.string()).mesh;
        test_map_attribute_types(mesh, 1);
        test_map_attribute_types(mesh, 4);
        mesh.initialize_edges();
        test_map_attribute_types(mesh, 1);
        test_map_attribute_types(mesh, 4);
    }
}

template <typename Scalar, typename Index, typename ValueType>
void test_map_attribute_invalid()
{
    using MeshType = lagrange::SurfaceMesh<Scalar, Index>;

    auto filenames = {
        "poly/L-plane.obj",
        "poly/mixedFaringPart.obj",
        "poly/tetris.obj",
        "cube_soup.obj"};

    for (fs::path filename : filenames) {
        fs::path input_path = lagrange::testing::get_data_path("open/core" / filename);
        REQUIRE(fs::exists(input_path));
        auto mesh = lagrange::io::load_mesh_obj<MeshType>(input_path.string()).mesh;
        mesh.initialize_edges();
        test_map_attribute_value_invalid<ValueType>(mesh, 1);
    }
}

} // namespace

TEST_CASE("map_attribute", "[attribute][conversion]" LA_SLOW_DEBUG_FLAG)
{
#define LA_X_map_attribute_all(_, Scalar, Index) test_map_attribute_all<Scalar, Index>();
    LA_SURFACE_MESH_X(map_attribute_all, 0)
}

TEST_CASE("map_attribute: invalid", "[attribute][conversion]")
{
    test_map_attribute_invalid<float, uint32_t, double>();
}

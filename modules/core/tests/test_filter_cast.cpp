/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/Attribute.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/cast.h>
#include <lagrange/filter_attributes.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("filter", "[core][surface][filter]")
{
    using Scalar = double;
    using Index = uint32_t;

    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(2, 1, 3);

    mesh.create_attribute<Scalar>(
        "a",
        lagrange::AttributeElement::Vertex,
        lagrange::AttributeUsage::Scalar);
    mesh.create_attribute<Scalar>(
        "b",
        lagrange::AttributeElement::Facet,
        lagrange::AttributeUsage::Color);

    SECTION("edges")
    {
        mesh.initialize_edges();
        REQUIRE(mesh.has_edges());

        auto filtered1 = lagrange::filter_attributes(mesh);
        REQUIRE(filtered1.has_edges());

        lagrange::AttributeFilter filter;
        filter.included_element_types = ~lagrange::BitField(lagrange::AttributeElement::Edge);

        auto filtered2 = lagrange::filter_attributes(mesh, filter);
        REQUIRE(!filtered2.has_edges());
    }

    SECTION("include basic")
    {
        lagrange::AttributeFilter filter;
        filter.included_attributes = {"a"};
        auto filtered = lagrange::filter_attributes(mesh, filter);
        REQUIRE(filtered.has_attribute("a"));
    }
    SECTION("include and exclude")
    {
        lagrange::AttributeFilter filter;
        filter.included_attributes = {"a"};
        filter.excluded_attributes = {"a"};
        auto filtered = lagrange::filter_attributes(mesh, filter);
        REQUIRE(!filtered.has_attribute("a"));
    }
    SECTION("include and not usage")
    {
        lagrange::AttributeFilter filter;
        filter.included_attributes = {"a"};
        filter.included_usages = ~lagrange::BitField(lagrange::AttributeUsage::Scalar);
        auto filtered = lagrange::filter_attributes(mesh, filter);
        REQUIRE(!filtered.has_attribute("a"));
    }
    SECTION("include and not element type")
    {
        lagrange::AttributeFilter filter;
        filter.included_attributes = {"a"};
        filter.included_element_types = ~lagrange::BitField(lagrange::AttributeElement::Vertex);
        auto filtered = lagrange::filter_attributes(mesh, filter);
        REQUIRE(!filtered.has_attribute("a"));
    }
    SECTION("exclude and usage")
    {
        lagrange::AttributeFilter filter;
        filter.excluded_attributes = {"a"};
        filter.included_usages = lagrange::BitField(lagrange::AttributeUsage::Scalar);
        auto filtered = lagrange::filter_attributes(mesh, filter);
        REQUIRE(!filtered.has_attribute("a"));
    }
    SECTION("exclude and element type")
    {
        lagrange::AttributeFilter filter;
        filter.excluded_attributes = {"a"};
        filter.included_element_types = lagrange::BitField(lagrange::AttributeElement::Vertex);
        auto filtered = lagrange::filter_attributes(mesh, filter);
        REQUIRE(!filtered.has_attribute("a"));
    }
    SECTION("include empty")
    {
        lagrange::AttributeFilter filter;
        filter.included_attributes.emplace();
        auto filtered = lagrange::filter_attributes(mesh, filter);
        REQUIRE(!filtered.has_attribute("a"));
        REQUIRE(!filtered.has_attribute("b"));
    }
}

TEST_CASE("cast basic", "[core][surface][cast]")
{
    using Scalar = double;
    using Index = uint32_t;

    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(2, 1, 3);

    SECTION("geometry only")
    {
        lagrange::SurfaceMesh<float, uint64_t> mesh2 = lagrange::cast<float, uint64_t>(mesh);

        REQUIRE(mesh2.get_num_vertices() == 4);
        REQUIRE(mesh2.get_num_facets() == 2);

        auto from_vertices = vertex_view(mesh);
        auto from_facets = facet_view(mesh);
        auto to_vertices = vertex_view(mesh2);
        auto to_facets = facet_view(mesh2);

        REQUIRE_THAT(
            (from_vertices - to_vertices.cast<Scalar>()).norm(),
            Catch::Matchers::WithinAbs(0, 1e-6));
        REQUIRE((from_facets - to_facets.cast<Index>()).cwiseAbs().maxCoeff() == 0);
    }

    SECTION("with uv")
    {
        std::array<Scalar, 4 * 2> uvs{0, 0, 1, 0, 0, 1, 1, 1};
        std::array<Index, 6> uv_indices{0, 1, 2, 2, 1, 3};
        mesh.create_attribute<Scalar>(
            "uv",
            lagrange::AttributeElement::Indexed,
            lagrange::AttributeUsage::UV,
            2,
            uvs,
            uv_indices);

        lagrange::SurfaceMesh<float, uint64_t> mesh2 = lagrange::cast<float, uint64_t>(mesh);
        REQUIRE(mesh2.has_attribute("uv"));

        auto& uv_attr = mesh2.get_indexed_attribute<float>("uv");
        auto& uv_value_attr = uv_attr.values();
        auto& uv_indices_attr = uv_attr.indices();

        REQUIRE(uv_value_attr.get_num_elements() == 4);
        for (Index i = 0; i < 4; i++) {
            REQUIRE_THAT(
                static_cast<Scalar>(uv_value_attr.get(i, 0)),
                Catch::Matchers::WithinAbs(uvs[i * 2], 1e-6));
            REQUIRE_THAT(
                static_cast<Scalar>(uv_value_attr.get(i, 1)),
                Catch::Matchers::WithinAbs(uvs[i * 2 + 1], 1e-6));
        }
        REQUIRE(uv_indices_attr.get_num_elements() == 6);
        for (Index i = 0; i < 6; i++) {
            REQUIRE(static_cast<Index>(uv_indices_attr.get(i)) == uv_indices[i]);
        }
    }
}

namespace {

template <typename ScalarA, typename IndexA, typename ScalarB, typename IndexB>
bool is_same_addr(
    const lagrange::SurfaceMesh<ScalarA, IndexA>& mesh_a,
    const lagrange::SurfaceMesh<ScalarB, IndexB>& mesh_b,
    std::string_view name)
{
    auto& attr_a = mesh_a.get_attribute_base(name);
    auto& attr_b = mesh_b.get_attribute_base(name);
    return reinterpret_cast<const void*>(&attr_a) == reinterpret_cast<const void*>(&attr_b);
}

} // namespace

TEST_CASE("cast address", "[core][surface][cast]")
{
    using Scalar = double;
    using Index = uint32_t;
    using OtherScalar = float;
    using OtherIndex = uint64_t;

    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(2, 1, 3);

    {
        // 0.3f - 0.2f --> 0x1.99999cp-4 (float)
        // 0.3  - 0.2  --> 0x1.9999999999998p-4 (double)
        std::array<Scalar, 4 * 2> uvs{0x1.9999999999998p-4, 0, 1, 0, 0, 1, 1, 1};
        std::array<Index, 6> uv_indices{0, 1, 2, 2, 1, 3};
        mesh.create_attribute<Scalar>(
            "uv",
            lagrange::AttributeElement::Indexed,
            lagrange::AttributeUsage::UV,
            2,
            uvs,
            uv_indices);
    }

    auto positions = lagrange::SurfaceMesh<Scalar, Index>::attr_name_vertex_to_position();
    auto indices = lagrange::SurfaceMesh<Scalar, Index>::attr_name_corner_to_vertex();
    auto uvs = std::string_view("uv");

    SECTION("same scalar, same index")
    {
        auto mesh2 = lagrange::cast<Scalar, Index>(mesh);
        REQUIRE(is_same_addr(mesh, mesh2, positions));
        REQUIRE(is_same_addr(mesh, mesh2, indices));
        REQUIRE(is_same_addr(mesh, mesh2, uvs));
    }

    SECTION("same scalar, different index")
    {
        auto mesh2 = lagrange::cast<Scalar, OtherIndex>(mesh);
        REQUIRE(is_same_addr(mesh, mesh2, positions));
        REQUIRE(!is_same_addr(mesh, mesh2, indices));
        REQUIRE(!is_same_addr(mesh, mesh2, uvs));
        const auto& attr_uv = mesh.get_indexed_attribute<Scalar>(uvs);
        const auto& attr_uv2 = mesh2.get_indexed_attribute<Scalar>(uvs);
        REQUIRE(matrix_view(attr_uv.values()) == matrix_view(attr_uv2.values()));
        REQUIRE(matrix_view(attr_uv.indices()) == matrix_view(attr_uv2.indices()).cast<Index>());
    }

    SECTION("different scalar, same index")
    {
        auto mesh2 = lagrange::cast<OtherScalar, Index>(mesh);
        REQUIRE(!is_same_addr(mesh, mesh2, positions));
        REQUIRE(is_same_addr(mesh, mesh2, indices));
        REQUIRE(!is_same_addr(mesh, mesh2, uvs));
        const auto& attr_uv = mesh.get_indexed_attribute<Scalar>(uvs);
        const auto& attr_uv2 = mesh2.get_indexed_attribute<OtherScalar>(uvs);
        // Casting to float should yield the same value
        REQUIRE(
            matrix_view(attr_uv.values()).cast<OtherScalar>() == matrix_view(attr_uv2.values()));
        // Casting to double should yield different value
        REQUIRE(matrix_view(attr_uv.values()) != matrix_view(attr_uv2.values()).cast<Scalar>());
        REQUIRE(matrix_view(attr_uv.indices()) == matrix_view(attr_uv2.indices()));
    }

    SECTION("different scalar, different index")
    {
        auto mesh2 = lagrange::cast<OtherScalar, OtherIndex>(mesh);
        REQUIRE(!is_same_addr(mesh, mesh2, positions));
        REQUIRE(!is_same_addr(mesh, mesh2, indices));
        REQUIRE(!is_same_addr(mesh, mesh2, uvs));
        const auto& attr_uv = mesh.get_indexed_attribute<Scalar>(uvs);
        const auto& attr_uv2 = mesh2.get_indexed_attribute<OtherScalar>(uvs);
        // Casting to float should yield the same value
        REQUIRE(
            matrix_view(attr_uv.values()).cast<OtherScalar>() == matrix_view(attr_uv2.values()));
        // Casting to double should yield different value
        REQUIRE(matrix_view(attr_uv.values()) != matrix_view(attr_uv2.values()).cast<Scalar>());
        REQUIRE(matrix_view(attr_uv.indices()) == matrix_view(attr_uv2.indices()).cast<Index>());
    }
}

TEST_CASE("cast external", "[core][surface][cast]")
{
    using Scalar = double;
    using Index = uint32_t;
    using OtherScalar = float;

    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(2, 1, 3);

    std::array<Scalar, 4> colors_values = {0.1, 0.2, 0.3, 0.4};
    mesh.create_attribute<Scalar>(
        "colors",
        lagrange::AttributeElement::Vertex,
        lagrange::AttributeUsage::Color,
        1,
        colors_values);
    mesh.wrap_as_attribute<Scalar>(
        "colors_rw",
        lagrange::AttributeElement::Vertex,
        lagrange::AttributeUsage::Color,
        1,
        colors_values);
    mesh.wrap_as_const_attribute<Scalar>(
        "colors_ro",
        lagrange::AttributeElement::Vertex,
        lagrange::AttributeUsage::Color,
        1,
        colors_values);

    auto colors_buffer = std::make_shared<std::array<Scalar, 4>>(colors_values);
    auto colors_view =
        lagrange::make_shared_span(colors_buffer, colors_buffer->data(), colors_buffer->size());
    mesh.wrap_as_attribute<Scalar>(
        "colors_sh",
        lagrange::AttributeElement::Vertex,
        lagrange::AttributeUsage::Color,
        1,
        colors_view);

    SECTION("same scalar")
    {
        auto mesh2 = lagrange::cast<Scalar, Index>(mesh);

        const auto& colors = mesh2.get_attribute<Scalar>("colors");
        REQUIRE(is_same_addr(mesh, mesh2, "colors"));
        REQUIRE(!colors.is_external());
        REQUIRE(colors.is_managed());
        REQUIRE(!colors.is_read_only());

        const auto& colors_rw = mesh2.get_attribute<Scalar>("colors_rw");
        REQUIRE(is_same_addr(mesh, mesh2, "colors_rw"));
        REQUIRE(colors_rw.is_external());
        REQUIRE(!colors_rw.is_managed());
        REQUIRE(!colors_rw.is_read_only());

        const auto& colors_ro = mesh2.get_attribute<Scalar>("colors_ro");
        REQUIRE(is_same_addr(mesh, mesh2, "colors_ro"));
        REQUIRE(colors_ro.is_external());
        REQUIRE(!colors_ro.is_managed());
        REQUIRE(colors_ro.is_read_only());

        const auto& colors_sh = mesh2.get_attribute<Scalar>("colors_sh");
        REQUIRE(is_same_addr(mesh, mesh2, "colors_sh"));
        REQUIRE(colors_sh.is_external());
        REQUIRE(colors_sh.is_managed());
        REQUIRE(!colors_sh.is_read_only());
    }

    SECTION("different scalar")
    {
        auto mesh2 = lagrange::cast<OtherScalar, Index>(mesh);

        const auto& colors = mesh2.get_attribute<Scalar>("colors");
        REQUIRE(!is_same_addr(mesh, mesh2, "colors"));
        REQUIRE(!colors.is_external());
        REQUIRE(colors.is_managed());
        REQUIRE(!colors.is_read_only());

        const auto& colors_rw = mesh2.get_attribute<Scalar>("colors_rw");
        REQUIRE(!is_same_addr(mesh, mesh2, "colors_rw"));
        REQUIRE(!colors_rw.is_external());
        REQUIRE(colors_rw.is_managed());
        REQUIRE(!colors_rw.is_read_only());

        const auto& colors_ro = mesh2.get_attribute<Scalar>("colors_ro");
        REQUIRE(!is_same_addr(mesh, mesh2, "colors_ro"));
        REQUIRE(!colors_ro.is_external());
        REQUIRE(colors_ro.is_managed());
        REQUIRE(!colors_ro.is_read_only());

        const auto& colors_sh = mesh2.get_attribute<Scalar>("colors_sh");
        REQUIRE(!is_same_addr(mesh, mesh2, "colors_sh"));
        REQUIRE(!colors_sh.is_external());
        REQUIRE(colors_sh.is_managed());
        REQUIRE(!colors_sh.is_read_only());
    }
}

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
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/cast.h>
#include <lagrange/cast_attribute.h>
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

template <typename Scalar, typename Index>
bool is_same_addr(
    const lagrange::SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name_a,
    std::string_view name_b)
{
    auto& attr_a = mesh.get_attribute_base(name_a);
    auto& attr_b = mesh.get_attribute_base(name_b);
    return reinterpret_cast<const void*>(&attr_a) == reinterpret_cast<const void*>(&attr_b);
}

template <typename Scalar, typename Index>
bool is_same_ptr(
    const lagrange::SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name,
    const void* ptr)
{
    auto& attr = mesh.get_attribute_base(name);
    return reinterpret_cast<const void*>(&attr) == ptr;
}

template <typename Scalar, typename Index>
const void* get_addr(const lagrange::SurfaceMesh<Scalar, Index>& mesh, std::string_view name)
{
    auto& attr = mesh.get_attribute_base(name);
    return reinterpret_cast<const void*>(&attr);
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
        const auto& attr_pos2 = mesh2.get_attribute<OtherScalar>(positions);
        const auto& attr_uv = mesh.get_indexed_attribute<Scalar>(uvs);
        const auto& attr_uv2 = mesh2.get_indexed_attribute<OtherScalar>(uvs);
        // Positions should be the same either way
        REQUIRE(vertex_view(mesh).cast<OtherScalar>() == matrix_view(attr_pos2));
        REQUIRE(vertex_view(mesh) == matrix_view(attr_pos2).cast<Scalar>());
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
        const auto& attr_pos2 = mesh2.get_attribute<OtherScalar>(positions);
        const auto& attr_uv = mesh.get_indexed_attribute<Scalar>(uvs);
        const auto& attr_uv2 = mesh2.get_indexed_attribute<OtherScalar>(uvs);
        // Positions should be the same either way
        REQUIRE(vertex_view(mesh).cast<OtherScalar>() == matrix_view(attr_pos2));
        REQUIRE(vertex_view(mesh) == matrix_view(attr_pos2).cast<Scalar>());
        // Casting to float should yield the same value
        REQUIRE(
            matrix_view(attr_uv.values()).cast<OtherScalar>() == matrix_view(attr_uv2.values()));
        // Casting to double should yield different value
        REQUIRE(matrix_view(attr_uv.values()) != matrix_view(attr_uv2.values()).cast<Scalar>());
        REQUIRE(matrix_view(attr_uv.indices()) == matrix_view(attr_uv2.indices()).cast<Index>());
    }

    SECTION("different scalar, cast attr")
    {
        auto positions2_id = lagrange::cast_attribute<OtherScalar>(mesh, positions, "positions2");
        auto uv2_id = lagrange::cast_attribute<OtherScalar>(mesh, uvs, "uv2");
        const auto& attr_pos2 = mesh.get_attribute<OtherScalar>(positions2_id);
        const auto& attr_uv = mesh.get_indexed_attribute<Scalar>(uvs);
        const auto& attr_uv2 = mesh.get_indexed_attribute<OtherScalar>(uv2_id);
        // Positions should be the same either way
        REQUIRE(vertex_view(mesh).cast<OtherScalar>() == matrix_view(attr_pos2));
        REQUIRE(vertex_view(mesh) == matrix_view(attr_pos2).cast<Scalar>());
        // Casting to float should yield the same value
        REQUIRE(
            matrix_view(attr_uv.values()).cast<OtherScalar>() == matrix_view(attr_uv2.values()));
        // Casting to double should yield different value
        REQUIRE(matrix_view(attr_uv.values()) != matrix_view(attr_uv2.values()).cast<Scalar>());
        REQUIRE(matrix_view(attr_uv.indices()) == matrix_view(attr_uv2.indices()));
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

    SECTION("same scalar, cast mesh")
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

    SECTION("same scalar, cast attr")
    {
        lagrange::cast_attribute<Scalar>(mesh, "colors", "colors2");
        const auto& colors = mesh.get_attribute<Scalar>("colors2");
        REQUIRE(is_same_addr(mesh, "colors", "colors2"));
        REQUIRE(!colors.is_external());
        REQUIRE(colors.is_managed());
        REQUIRE(!colors.is_read_only());

        lagrange::cast_attribute<Scalar>(mesh, "colors_rw", "colors_rw2");
        const auto& colors_rw = mesh.get_attribute<Scalar>("colors_rw2");
        REQUIRE(is_same_addr(mesh, "colors_rw", "colors_rw2"));
        REQUIRE(colors_rw.is_external());
        REQUIRE(!colors_rw.is_managed());
        REQUIRE(!colors_rw.is_read_only());

        lagrange::cast_attribute<Scalar>(mesh, "colors_ro", "colors_ro2");
        const auto& colors_ro = mesh.get_attribute<Scalar>("colors_ro2");
        REQUIRE(is_same_addr(mesh, "colors_ro", "colors_ro2"));
        REQUIRE(colors_ro.is_external());
        REQUIRE(!colors_ro.is_managed());
        REQUIRE(colors_ro.is_read_only());

        lagrange::cast_attribute<Scalar>(mesh, "colors_sh", "colors_sh2");
        const auto& colors_sh = mesh.get_attribute<Scalar>("colors_sh2");
        REQUIRE(is_same_addr(mesh, "colors_sh", "colors_sh2"));
        REQUIRE(colors_sh.is_external());
        REQUIRE(colors_sh.is_managed());
        REQUIRE(!colors_sh.is_read_only());
    }

    SECTION("same scalar, cast attr in place")
    {
        auto colors_ptr = get_addr(mesh, "colors");
        lagrange::cast_attribute_in_place<Scalar>(mesh, "colors");
        const auto& colors = mesh.get_attribute<Scalar>("colors");
        REQUIRE(is_same_ptr(mesh, "colors", colors_ptr));
        REQUIRE(!colors.is_external());
        REQUIRE(colors.is_managed());
        REQUIRE(!colors.is_read_only());

        auto colors_rw_ptr = get_addr(mesh, "colors_rw");
        lagrange::cast_attribute_in_place<Scalar>(mesh, "colors_rw");
        const auto& colors_rw = mesh.get_attribute<Scalar>("colors_rw");
        REQUIRE(is_same_ptr(mesh, "colors_rw", colors_rw_ptr));
        REQUIRE(colors_rw.is_external());
        REQUIRE(!colors_rw.is_managed());
        REQUIRE(!colors_rw.is_read_only());

        auto colors_ro_ptr = get_addr(mesh, "colors_ro");
        lagrange::cast_attribute_in_place<Scalar>(mesh, "colors_ro");
        const auto& colors_ro = mesh.get_attribute<Scalar>("colors_ro");
        REQUIRE(is_same_ptr(mesh, "colors_ro", colors_ro_ptr));
        REQUIRE(colors_ro.is_external());
        REQUIRE(!colors_ro.is_managed());
        REQUIRE(colors_ro.is_read_only());

        auto colors_sh_ptr = get_addr(mesh, "colors_sh");
        lagrange::cast_attribute_in_place<Scalar>(mesh, "colors_sh");
        const auto& colors_sh = mesh.get_attribute<Scalar>("colors_sh");
        REQUIRE(is_same_ptr(mesh, "colors_sh", colors_sh_ptr));
        REQUIRE(colors_sh.is_external());
        REQUIRE(colors_sh.is_managed());
        REQUIRE(!colors_sh.is_read_only());
    }

    SECTION("different scalar, cast mesh")
    {
        auto mesh2 = lagrange::cast<OtherScalar, Index>(mesh);

        const auto& colors = mesh2.get_attribute<OtherScalar>("colors");
        REQUIRE(!is_same_addr(mesh, mesh2, "colors"));
        REQUIRE(!colors.is_external());
        REQUIRE(colors.is_managed());
        REQUIRE(!colors.is_read_only());

        const auto& colors_rw = mesh2.get_attribute<OtherScalar>("colors_rw");
        REQUIRE(!is_same_addr(mesh, mesh2, "colors_rw"));
        REQUIRE(!colors_rw.is_external());
        REQUIRE(colors_rw.is_managed());
        REQUIRE(!colors_rw.is_read_only());

        const auto& colors_ro = mesh2.get_attribute<OtherScalar>("colors_ro");
        REQUIRE(!is_same_addr(mesh, mesh2, "colors_ro"));
        REQUIRE(!colors_ro.is_external());
        REQUIRE(colors_ro.is_managed());
        REQUIRE(!colors_ro.is_read_only());

        const auto& colors_sh = mesh2.get_attribute<OtherScalar>("colors_sh");
        REQUIRE(!is_same_addr(mesh, mesh2, "colors_sh"));
        REQUIRE(!colors_sh.is_external());
        REQUIRE(colors_sh.is_managed());
        REQUIRE(!colors_sh.is_read_only());
    }

    SECTION("different scalar, cast attr")
    {
        lagrange::cast_attribute<OtherScalar>(mesh, "colors", "colors2");
        const auto& colors = mesh.get_attribute<OtherScalar>("colors2");
        REQUIRE(!is_same_addr(mesh, "colors", "colors2"));
        REQUIRE(!colors.is_external());
        REQUIRE(colors.is_managed());
        REQUIRE(!colors.is_read_only());

        lagrange::cast_attribute<OtherScalar>(mesh, "colors_rw", "colors_rw2");
        const auto& colors_rw = mesh.get_attribute<OtherScalar>("colors_rw2");
        REQUIRE(!is_same_addr(mesh, "colors_rw", "colors_rw2"));
        REQUIRE(!colors_rw.is_external());
        REQUIRE(colors_rw.is_managed());
        REQUIRE(!colors_rw.is_read_only());

        lagrange::cast_attribute<OtherScalar>(mesh, "colors_ro", "colors_ro2");
        const auto& colors_ro = mesh.get_attribute<OtherScalar>("colors_ro2");
        REQUIRE(!is_same_addr(mesh, "colors_ro", "colors_ro2"));
        REQUIRE(!colors_ro.is_external());
        REQUIRE(colors_ro.is_managed());
        REQUIRE(!colors_ro.is_read_only());

        lagrange::cast_attribute<OtherScalar>(mesh, "colors_sh", "colors_sh2");
        const auto& colors_sh = mesh.get_attribute<OtherScalar>("colors_sh2");
        REQUIRE(!is_same_addr(mesh, "colors_sh", "colors_sh2"));
        REQUIRE(!colors_sh.is_external());
        REQUIRE(colors_sh.is_managed());
        REQUIRE(!colors_sh.is_read_only());
    }

    SECTION("different scalar, cast attr in place")
    {
        // The Attribute object address could be the same, or different... no guarantee in this case
        lagrange::cast_attribute_in_place<OtherScalar>(mesh, "colors");
        const auto& colors = mesh.get_attribute<OtherScalar>("colors");
        REQUIRE(!colors.is_external());
        REQUIRE(colors.is_managed());
        REQUIRE(!colors.is_read_only());

        lagrange::cast_attribute_in_place<OtherScalar>(mesh, "colors_rw");
        const auto& colors_rw = mesh.get_attribute<OtherScalar>("colors_rw");
        REQUIRE(!colors_rw.is_external());
        REQUIRE(colors_rw.is_managed());
        REQUIRE(!colors_rw.is_read_only());

        lagrange::cast_attribute_in_place<OtherScalar>(mesh, "colors_ro");
        const auto& colors_ro = mesh.get_attribute<OtherScalar>("colors_ro");
        REQUIRE(!colors_ro.is_external());
        REQUIRE(colors_ro.is_managed());
        REQUIRE(!colors_ro.is_read_only());

        lagrange::cast_attribute_in_place<OtherScalar>(mesh, "colors_sh");
        const auto& colors_sh = mesh.get_attribute<OtherScalar>("colors_sh");
        REQUIRE(!colors_sh.is_external());
        REQUIRE(colors_sh.is_managed());
        REQUIRE(!colors_sh.is_read_only());
    }
}


TEST_CASE("cast invalid", "[core][surface][cast]")
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

    std::array<Scalar, 4> colors_values = {0.1, 0.2, 0.3, lagrange::invalid<Scalar>()};
    auto colors_id = mesh.create_attribute<Scalar>(
        "colors",
        lagrange::AttributeElement::Vertex,
        lagrange::AttributeUsage::Color,
        1,
        colors_values);

    std::array<Index, 4> group_values = {0, lagrange::invalid<Index>(), 2, 100};
    auto groups_id = mesh.create_attribute<Index>(
        "groups",
        lagrange::AttributeElement::Vertex,
        lagrange::AttributeUsage::Scalar,
        1,
        group_values);

    std::array<Index, 4> v2f_values = {0, 1, lagrange::invalid<Index>(), 0};
    auto v2f_id = mesh.create_attribute<Index>(
        "v2f",
        lagrange::AttributeElement::Vertex,
        lagrange::AttributeUsage::FacetIndex,
        1,
        v2f_values);

    SECTION("default remap") {
        auto other_mesh = lagrange::cast<OtherScalar, OtherIndex>(mesh);
        auto colors = lagrange::attribute_vector_view<OtherScalar>(other_mesh, "colors");
        auto groups = lagrange::attribute_vector_view<OtherIndex>(other_mesh, "groups");
        auto v2f = lagrange::attribute_vector_view<OtherIndex>(other_mesh, "v2f");
        // invalid<float>() and invalid<double>() are the same
        REQUIRE(colors[3] == static_cast<OtherScalar>(lagrange::invalid<Scalar>()));
        REQUIRE(colors[3] == lagrange::invalid<OtherScalar>());
        REQUIRE(groups[1] == static_cast<OtherIndex>(lagrange::invalid<Index>()));
        REQUIRE(groups[1] != lagrange::invalid<OtherIndex>());
        REQUIRE(v2f[2] != static_cast<OtherIndex>(lagrange::invalid<Index>()));
        REQUIRE(v2f[2] == lagrange::invalid<OtherIndex>());
    }

    SECTION("always remap") {
        mesh.ref_attribute<Scalar>(colors_id).set_cast_policy(lagrange::AttributeCastPolicy::RemapInvalidAlways);
        mesh.ref_attribute<Scalar>(groups_id).set_cast_policy(lagrange::AttributeCastPolicy::RemapInvalidAlways);
        mesh.ref_attribute<Scalar>(v2f_id).set_cast_policy(lagrange::AttributeCastPolicy::RemapInvalidAlways);
        auto other_mesh = lagrange::cast<OtherScalar, OtherIndex>(mesh);
        auto colors = lagrange::attribute_vector_view<OtherScalar>(other_mesh, "colors");
        auto groups = lagrange::attribute_vector_view<OtherIndex>(other_mesh, "groups");
        auto v2f = lagrange::attribute_vector_view<OtherIndex>(other_mesh, "v2f");
        // invalid<float>() and invalid<double>() are the same
        REQUIRE(colors[3] == static_cast<OtherScalar>(lagrange::invalid<Scalar>()));
        REQUIRE(colors[3] == lagrange::invalid<OtherScalar>());
        REQUIRE(groups[1] != static_cast<OtherIndex>(lagrange::invalid<Index>()));
        REQUIRE(groups[1] == lagrange::invalid<OtherIndex>());
        REQUIRE(v2f[2] != static_cast<OtherIndex>(lagrange::invalid<Index>()));
        REQUIRE(v2f[2] == lagrange::invalid<OtherIndex>());
    }

    SECTION("never remap") {
        mesh.ref_attribute<Scalar>(colors_id).set_cast_policy(lagrange::AttributeCastPolicy::DoNotRemapInvalid);
        mesh.ref_attribute<Scalar>(groups_id).set_cast_policy(lagrange::AttributeCastPolicy::DoNotRemapInvalid);
        mesh.ref_attribute<Scalar>(v2f_id).set_cast_policy(lagrange::AttributeCastPolicy::DoNotRemapInvalid);
        auto other_mesh = lagrange::cast<OtherScalar, OtherIndex>(mesh);
        auto colors = lagrange::attribute_vector_view<OtherScalar>(other_mesh, "colors");
        auto groups = lagrange::attribute_vector_view<OtherIndex>(other_mesh, "groups");
        auto v2f = lagrange::attribute_vector_view<OtherIndex>(other_mesh, "v2f");
        // invalid<float>() and invalid<double>() are the same
        REQUIRE(colors[3] == static_cast<OtherScalar>(lagrange::invalid<Scalar>()));
        REQUIRE(colors[3] == lagrange::invalid<OtherScalar>());
        REQUIRE(groups[1] == static_cast<OtherIndex>(lagrange::invalid<Index>()));
        REQUIRE(groups[1] != lagrange::invalid<OtherIndex>());
        REQUIRE(v2f[2] == static_cast<OtherIndex>(lagrange::invalid<Index>()));
        REQUIRE(v2f[2] != lagrange::invalid<OtherIndex>());
    }
}

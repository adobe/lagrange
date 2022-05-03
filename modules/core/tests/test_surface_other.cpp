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
#include <lagrange/Attribute.h>
#include <lagrange/AttributeTypes.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/testing/common.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <catch2/catch.hpp>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <array>
#include <iostream>

namespace {

template <typename Scalar>
bool is_same(lagrange::span<const Scalar> p, std::array<Scalar, 3> q)
{
    for (size_t i = 0; i < 3; ++i) {
        if (p[i] != q[i]) {
            return false;
        }
    }
    return true;
}

template <typename Scalar, typename Index>
void test_cow_mesh()
{
    using Point = std::array<Scalar, 3>;

    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(4, [](Index i, lagrange::span<Scalar> p) {
        p[0] = Scalar(3 * i);
        p[1] = Scalar(3 * i + 1);
        p[2] = Scalar(3 * i + 2);
    });
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(2, 3, 0);
    REQUIRE(is_same(mesh.get_position(0), Point{{0., 1., 2.}}));
    REQUIRE(is_same(mesh.get_position(1), Point{{3., 4., 5.}}));
    REQUIRE(is_same(mesh.get_position(2), Point{{6., 7., 8.}}));
    REQUIRE(is_same(mesh.get_position(3), Point{{9., 10., 11.}}));
    REQUIRE(mesh.get_corner_vertex(0) == 0);
    REQUIRE(mesh.get_corner_vertex(1) == 1);
    REQUIRE(mesh.get_corner_vertex(2) == 2);
    REQUIRE(mesh.get_corner_vertex(3) == 2);
    REQUIRE(mesh.get_corner_vertex(4) == 3);
    REQUIRE(mesh.get_corner_vertex(5) == 0);

    lagrange::SurfaceMesh copy = mesh;
    REQUIRE(is_same(copy.get_position(0), Point{{0., 1., 2.}}));
    REQUIRE(is_same(copy.get_position(1), Point{{3., 4., 5.}}));
    REQUIRE(is_same(copy.get_position(2), Point{{6., 7., 8.}}));
    REQUIRE(is_same(copy.get_position(3), Point{{9., 10., 11.}}));
    REQUIRE(copy.get_corner_vertex(0) == 0);
    REQUIRE(copy.get_corner_vertex(1) == 1);
    REQUIRE(copy.get_corner_vertex(2) == 2);
    REQUIRE(copy.get_corner_vertex(3) == 2);
    REQUIRE(copy.get_corner_vertex(4) == 3);
    REQUIRE(copy.get_corner_vertex(5) == 0);

    {
        auto p = copy.ref_position(1);
        p[0] = Scalar(0.1);
        p[1] = Scalar(0.2);
        p[2] = Scalar(0.3);
    }
    REQUIRE(is_same(mesh.get_position(0), Point{{0., 1., 2.}}));
    REQUIRE(is_same(mesh.get_position(1), Point{{3., 4., 5.}})); // <-- old mesh data is unchanged
    REQUIRE(is_same(mesh.get_position(2), Point{{6., 7., 8.}}));
    REQUIRE(is_same(mesh.get_position(3), Point{{9., 10., 11.}}));
    REQUIRE(is_same(copy.get_position(0), Point{{0., 1., 2.}}));
    REQUIRE(is_same(copy.get_position(1), Point{{Scalar(0.1), Scalar(0.2), Scalar(0.3)}}));
    REQUIRE(is_same(copy.get_position(2), Point{{6., 7., 8.}}));
    REQUIRE(is_same(copy.get_position(3), Point{{9., 10., 11.}}));
    REQUIRE(copy.get_corner_vertex(0) == 0);
    REQUIRE(copy.get_corner_vertex(1) == 1);
    REQUIRE(copy.get_corner_vertex(2) == 2);
    REQUIRE(copy.get_corner_vertex(3) == 2);
    REQUIRE(copy.get_corner_vertex(4) == 3);
    REQUIRE(copy.get_corner_vertex(5) == 0);
}

template <typename ValueType, typename Scalar, typename Index>
void test_cow_attribute()
{
    using namespace lagrange;
    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(10);
    mesh.template create_attribute<ValueType>("foo", AttributeElement::Vertex);

    {
        SurfaceMesh<Scalar, Index> copy = mesh;
        REQUIRE(
            mesh.template get_attribute<ValueType>("foo").get_all().data() ==
            copy.template get_attribute<ValueType>("foo").get_all().data());
        auto& attr = copy.template ref_attribute<ValueType>("foo");
        attr.ref(0, 0) = ValueType(1);
        REQUIRE(
            mesh.template get_attribute<ValueType>("foo").get_all().data() !=
            copy.template get_attribute<ValueType>("foo").get_all().data());
    }

    {
        SurfaceMesh copy = mesh;
        REQUIRE(
            mesh.template get_attribute<ValueType>("foo").get_all().data() ==
            copy.template get_attribute<ValueType>("foo").get_all().data());
        auto attr_ptr = copy.template delete_and_export_attribute<ValueType>("foo");
        REQUIRE(
            attr_ptr->get_all().data() !=
            mesh.template get_attribute<ValueType>("foo").get_all().data());
    }
}

template <typename Scalar, typename Index>
void test_span()
{
    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(4, [](Index i, lagrange::span<Scalar> p) {
        p[0] = Scalar(3 * i);
        p[1] = Scalar(3 * i + 1);
        p[2] = Scalar(3 * i + 2);
    });
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(2, 3, 0);

    auto facet_vertices = mesh.get_facet_vertices(1);
    REQUIRE(facet_vertices.size() == 3);
    REQUIRE(facet_vertices[0] == 2);
    REQUIRE(facet_vertices[1] == 3);
    REQUIRE(facet_vertices[2] == 0);
}

template <typename Scalar, typename Index>
void test_attributes()
{
    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(4, [](Index i, lagrange::span<Scalar> p) {
        p[0] = Scalar(3 * i);
        p[1] = Scalar(3 * i + 1);
        p[2] = Scalar(3 * i + 2);
    });
    mesh.add_triangle(0, 1, 2);
    mesh.add_quad(2, 3, 0, 1);

    auto foo_id = mesh.template create_attribute<float>("foo", lagrange::AttributeElement::Vertex);
    auto bar_id =
        mesh.template create_attribute<uint32_t>("bar", lagrange::AttributeElement::Facet);
    auto baz_id =
        mesh.template create_attribute<int64_t>("baz", lagrange::AttributeElement::Corner);
    LA_REQUIRE_THROWS(
        mesh.template create_attribute<float>("foo", lagrange::AttributeElement::Vertex));

    REQUIRE(mesh.get_num_vertices() == 4);

    REQUIRE(mesh.template get_attribute<float>(foo_id).get_num_channels() == 1);
    REQUIRE(mesh.template get_attribute<uint32_t>(bar_id).get_num_channels() == 1);
    REQUIRE(mesh.template get_attribute<int64_t>(baz_id).get_num_channels() == 1);

    REQUIRE(mesh.template get_attribute<float>(foo_id).get_num_elements() == 4);
    REQUIRE(mesh.template get_attribute<uint32_t>(bar_id).get_num_elements() == 2);
    REQUIRE(mesh.template get_attribute<int64_t>(baz_id).get_num_elements() == 7);
}

} // namespace

TEST_CASE("SurfaceMesh: Copy-on-write", "[next]")
{
#define LA_X_test_cow_mesh(_, Scalar, Index) test_cow_mesh<Scalar, Index>();
    LA_SURFACE_MESH_X(test_cow_mesh, 0)

#define LA_X_test_cow_attribute(ValueType, Scalar, Index) \
    test_cow_attribute<ValueType, Scalar, Index>();
#define LA_X_test_cow_attribute_aux(_, ValueType) LA_SURFACE_MESH_X(test_cow_attribute, ValueType)
    LA_ATTRIBUTE_X(test_cow_attribute_aux, 0)
}

TEST_CASE("SurfaceMesh: Span", "[next]")
{
#define LA_X_test_span(_, Scalar, Index) test_span<Scalar, Index>();
    LA_SURFACE_MESH_X(test_span, 0)
}

TEST_CASE("SurfaceMesh: Attributes", "[next]")
{
#define LA_X_test_attributes(_, Scalar, Index) test_attributes<Scalar, Index>();
    LA_SURFACE_MESH_X(test_attributes, 0)
}

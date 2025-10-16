/*
 * Copyright 2017 Adobe. All rights reserved.
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
#include <Eigen/Core>

#include <lagrange/attributes/attribute_utils.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/utils/range.h>

#include <catch2/catch_approx.hpp>

TEST_CASE("AttributeUtils", "[attribute][conversion]")
{
    using namespace lagrange;

    auto mesh = create_cube();

    using MeshType = decltype(mesh)::element_type;
    using AttributeArray = typename MeshType::AttributeArray;
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;

    const Index num_vertices = mesh->get_num_vertices();
    const Index num_facets = mesh->get_num_facets();
    const Index vertex_per_facet = mesh->get_vertex_per_facet();
    const auto& facets = mesh->get_facets();

    SECTION("map_vertex_attribute_to_corner_attribute")
    {
        AttributeArray vertex_indices(num_vertices, 1);
        for (Index i = 0; i < num_vertices; i++) {
            vertex_indices(i, 0) = safe_cast<int>(i);
        }
        mesh->add_vertex_attribute("index");
        mesh->import_vertex_attribute("index", vertex_indices);
        map_vertex_attribute_to_corner_attribute(*mesh, "index");

        REQUIRE(mesh->has_corner_attribute("index"));

        const auto& corner_indices = mesh->get_corner_attribute("index");
        REQUIRE(corner_indices.rows() == num_facets * vertex_per_facet);
        REQUIRE(corner_indices.cols() == 1);

        for (Index i = 0; i < num_facets; i++) {
            for (Index j = 0; j < vertex_per_facet; j++) {
                REQUIRE(corner_indices(i * vertex_per_facet + j, 0) == facets(i, j));
            }
        }

        SECTION("map_corner_attribute_to_vertex_attribute")
        {
            vertex_indices.setZero();
            mesh->import_vertex_attribute("index", vertex_indices);
            map_corner_attribute_to_vertex_attribute(*mesh, "index");
            mesh->export_vertex_attribute("index", vertex_indices);
            for (Index i = 0; i < num_vertices; i++) {
                REQUIRE(Catch::Approx(vertex_indices(i, 0)) == i);
            }
        }
    }

    SECTION("map_facet_attribute_to_corner_attribute")
    {
        AttributeArray facet_indices(num_facets, 1);
        for (Index i = 0; i < num_facets; i++) {
            facet_indices(i, 0) = safe_cast<Index>(i);
        }
        mesh->add_facet_attribute("index");
        mesh->import_facet_attribute("index", facet_indices);
        map_facet_attribute_to_corner_attribute(*mesh, "index");

        REQUIRE(mesh->has_corner_attribute("index"));

        const auto& corner_indices = mesh->get_corner_attribute("index");
        REQUIRE(corner_indices.rows() == num_facets * vertex_per_facet);
        REQUIRE(corner_indices.cols() == 1);

        for (Index i = 0; i < num_facets; i++) {
            for (Index j = 0; j < vertex_per_facet; j++) {
                REQUIRE(corner_indices(i * vertex_per_facet + j, 0) == i);
            }
        }

        SECTION("map_corner_attribute_to_facet_attribute")
        {
            facet_indices.setZero();
            mesh->import_facet_attribute("index", facet_indices);
            map_corner_attribute_to_facet_attribute(*mesh, "index");
            mesh->export_facet_attribute("index", facet_indices);
            for (auto i : range(num_facets)) {
                REQUIRE(facet_indices(i, 0) == Catch::Approx(i));
            }
        }
    }

    SECTION("map_vertex_attribute_to_indexed_attribute")
    {
        const std::string attr_name = "index";
        AttributeArray vertex_attr(num_vertices, 1);
        for (Index i = 0; i < num_vertices; i++) {
            vertex_attr(i, 0) = safe_cast<Scalar>(i);
        }
        mesh->add_vertex_attribute(attr_name);
        mesh->set_vertex_attribute(attr_name, vertex_attr);
        map_vertex_attribute_to_indexed_attribute(*mesh, attr_name);

        REQUIRE(mesh->has_indexed_attribute(attr_name));

        const auto entry = mesh->get_indexed_attribute(attr_name);
        const auto attr_values = std::get<0>(entry);
        const auto attr_indices = std::get<1>(entry);
        REQUIRE(attr_indices.rows() == num_facets);
        REQUIRE(attr_indices.cols() == vertex_per_facet);
        REQUIRE(attr_values.cols() == vertex_attr.cols());

        for (Index i = 0; i < num_facets; i++) {
            for (Index j = 0; j < vertex_per_facet; j++) {
                REQUIRE(attr_values.row(attr_indices(i, j)) == vertex_attr.row(facets(i, j)));
            }
        }

        SECTION("map_indexed_attribute_to_vertex_attribute")
        {
            mesh->remove_vertex_attribute(attr_name);
            REQUIRE(!mesh->has_vertex_attribute(attr_name));
            map_indexed_attribute_to_vertex_attribute(*mesh, attr_name);
            REQUIRE(mesh->has_vertex_attribute(attr_name));
            mesh->export_vertex_attribute(attr_name, vertex_attr);
            REQUIRE(vertex_attr.rows() == num_vertices);
            REQUIRE(vertex_attr.cols() == attr_values.cols());
            for (Index i = 0; i < num_vertices; i++) {
                REQUIRE(vertex_attr(i, 0) == i);
            }
        }
    }

    SECTION("map_facet_attribute_to_indexed_attribute")
    {
        const std::string attr_name = "index";
        AttributeArray facet_attr(num_facets, 1);
        for (Index i = 0; i < num_facets; i++) {
            facet_attr(i, 0) = safe_cast<Scalar>(i);
        }
        mesh->add_facet_attribute(attr_name);
        mesh->set_facet_attribute(attr_name, facet_attr);
        map_facet_attribute_to_indexed_attribute(*mesh, attr_name);

        REQUIRE(mesh->has_indexed_attribute(attr_name));

        const auto entry = mesh->get_indexed_attribute(attr_name);
        const auto attr_values = std::get<0>(entry);
        const auto attr_indices = std::get<1>(entry);
        REQUIRE(attr_indices.rows() == num_facets);
        REQUIRE(attr_indices.cols() == vertex_per_facet);
        REQUIRE(attr_values.cols() == facet_attr.cols());

        for (Index i = 0; i < num_facets; i++) {
            for (Index j = 0; j < vertex_per_facet; j++) {
                REQUIRE(attr_values.row(attr_indices(i, j)) == facet_attr.row(i));
            }
        }

        SECTION("map_indexed_attribute_to_facet_attribute")
        {
            mesh->remove_facet_attribute(attr_name);
            REQUIRE(!mesh->has_facet_attribute(attr_name));
            map_indexed_attribute_to_facet_attribute(*mesh, attr_name);
            REQUIRE(mesh->has_facet_attribute(attr_name));
            mesh->export_facet_attribute(attr_name, facet_attr);
            REQUIRE(facet_attr.rows() == num_facets);
            REQUIRE(facet_attr.cols() == attr_values.cols());
            for (Index i = 0; i < num_facets; i++) {
                REQUIRE(facet_attr(i, 0) == i);
            }
        }
    }

    SECTION("map_corner_attribute_to_indexed_attribute")
    {
        const std::string attr_name = "index";
        AttributeArray corner_attr(num_facets * vertex_per_facet, 1);
        for (Index i = 0; i < num_facets * vertex_per_facet; i++) {
            corner_attr(i, 0) = safe_cast<Scalar>(i);
        }
        mesh->add_corner_attribute(attr_name);
        mesh->set_corner_attribute(attr_name, corner_attr);
        map_corner_attribute_to_indexed_attribute(*mesh, attr_name);

        REQUIRE(mesh->has_indexed_attribute(attr_name));

        const auto entry = mesh->get_indexed_attribute(attr_name);
        const auto attr_values = std::get<0>(entry);
        const auto attr_indices = std::get<1>(entry);
        REQUIRE(attr_indices.rows() == num_facets);
        REQUIRE(attr_indices.cols() == vertex_per_facet);
        REQUIRE(attr_values.cols() == corner_attr.cols());

        for (Index i = 0; i < num_facets; i++) {
            for (Index j = 0; j < vertex_per_facet; j++) {
                REQUIRE(
                    attr_values.row(attr_indices(i, j)) ==
                    corner_attr.row(i * vertex_per_facet + j));
            }
        }

        SECTION("map_indexed_attribute_to_corner_attribute")
        {
            mesh->remove_corner_attribute(attr_name);
            REQUIRE(!mesh->has_corner_attribute(attr_name));
            map_indexed_attribute_to_corner_attribute(*mesh, attr_name);
            REQUIRE(mesh->has_corner_attribute(attr_name));
            mesh->export_corner_attribute(attr_name, corner_attr);
            REQUIRE(corner_attr.rows() == num_facets * vertex_per_facet);
            REQUIRE(corner_attr.cols() == attr_values.cols());
            for (Index i = 0; i < num_facets * vertex_per_facet; i++) {
                REQUIRE(corner_attr(i, 0) == i);
            }
        }
    }
}

TEST_CASE("AttributeConversions", "[attribute]")
{
    Eigen::Matrix<float, Eigen::Dynamic, 3> vertices(4, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0;
    Eigen::Matrix<int, Eigen::Dynamic, 3> facets(2, 3);
    facets << 0, 1, 2, 2, 1, 3;

    const auto mesh = lagrange::create_mesh(vertices, facets);
    const auto num_vertices = mesh->get_num_vertices();
    const auto num_facets = mesh->get_num_facets();
    const auto vertex_per_facet = mesh->get_vertex_per_facet();

    Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> uv(4, 2);
    uv << 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0;

    mesh->add_vertex_attribute("uv");
    mesh->import_vertex_attribute("uv", uv);
    map_vertex_attribute_to_corner_attribute(*mesh, "uv");

    REQUIRE(mesh->has_corner_attribute("uv"));
    const auto& corner_uv = mesh->get_corner_attribute("uv");
    REQUIRE(corner_uv.rows() == num_facets * vertex_per_facet);
    for (int i = 0; i < num_facets; i++) {
        const auto& f = facets.row(i);
        REQUIRE(corner_uv(i * 3, 0) == vertices(f[0], 0));
        REQUIRE(corner_uv(i * 3, 1) == vertices(f[0], 1));
        REQUIRE(corner_uv(i * 3 + 1, 0) == vertices(f[1], 0));
        REQUIRE(corner_uv(i * 3 + 1, 1) == vertices(f[1], 1));
        REQUIRE(corner_uv(i * 3 + 2, 0) == vertices(f[2], 0));
        REQUIRE(corner_uv(i * 3 + 2, 1) == vertices(f[2], 1));
    }

    mesh->remove_vertex_attribute("uv");
    REQUIRE(!mesh->has_vertex_attribute("uv"));

    map_corner_attribute_to_vertex_attribute(*mesh, "uv");
    REQUIRE(mesh->has_vertex_attribute("uv"));

    Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> uv2(4, 2);
    uv2.setZero();
    mesh->export_vertex_attribute("uv", uv2);
    mesh->remove_vertex_attribute("uv");
    REQUIRE(uv2.rows() == num_vertices);
    for (int i = 0; i < num_vertices; i++) {
        REQUIRE(uv2(i, 0) == vertices(i, 0));
        REQUIRE(uv2(i, 1) == vertices(i, 1));
    }
}

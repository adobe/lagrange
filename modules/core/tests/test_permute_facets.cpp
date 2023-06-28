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
#include <lagrange/Attribute.h>
#include <lagrange/permute_facets.h>
#include <lagrange/testing/check_mesh.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>


#include <Eigen/Core>
#include <catch2/benchmark/catch_benchmark.hpp>

TEST_CASE("permute_facets", "[surface][reorder_facets]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    using RowI = Eigen::Matrix<Index, 1, 3>;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(0, 2, 3);

    SECTION("identity")
    {
        std::vector<Index> order{0, 1};
        permute_facets<Scalar, Index>(mesh, order);
        auto new_facets = facet_view(mesh);

        REQUIRE((new_facets.row(0).array() == RowI(0, 1, 2).array()).all());
        REQUIRE((new_facets.row(1).array() == RowI(0, 2, 3).array()).all());
        lagrange::testing::check_mesh(mesh);
    }

    SECTION("reverse")
    {
        std::vector<Index> order{1, 0};
        permute_facets<Scalar, Index>(mesh, order);
        auto new_facets = facet_view(mesh);

        REQUIRE((new_facets.row(0).array() == RowI(0, 2, 3).array()).all());
        REQUIRE((new_facets.row(1).array() == RowI(0, 1, 2).array()).all());
        lagrange::testing::check_mesh(mesh);
    }

    SECTION("with facet attributes")
    {
        auto id = mesh.template create_attribute<int>(
            "facet_index",
            AttributeElement::Facet,
            AttributeUsage::Scalar);
        auto data = matrix_ref(mesh.ref_attribute<int>(id));
        data << 1, 2;

        std::vector<Index> order{1, 0};
        permute_facets<Scalar, Index>(mesh, order);
        REQUIRE(mesh.get_num_facets() == 2);

        const auto& attr = mesh.get_attribute<int>(id);
        REQUIRE(attr.get(0) == 2);
        REQUIRE(attr.get(1) == 1);
        lagrange::testing::check_mesh(mesh);
    }

    SECTION("with corner attributes")
    {
        auto id = mesh.template create_attribute<int>(
            "corner_index",
            AttributeElement::Corner,
            AttributeUsage::Scalar);
        auto data = matrix_ref(mesh.ref_attribute<int>(id));
        data << 1, 2, 3, 4, 5, 6;

        std::vector<Index> order{1, 0};
        permute_facets<Scalar, Index>(mesh, order);
        REQUIRE(mesh.get_num_facets() == 2);

        const auto& attr = mesh.get_attribute<int>(id);
        REQUIRE(attr.get(0) == 4);
        REQUIRE(attr.get(1) == 5);
        REQUIRE(attr.get(2) == 6);
        REQUIRE(attr.get(3) == 1);
        REQUIRE(attr.get(4) == 2);
        REQUIRE(attr.get(5) == 3);
        lagrange::testing::check_mesh(mesh);
    }

    SECTION("with connectivity")
    {
        mesh.initialize_edges();
        std::vector<Index> order{1, 0};
        permute_facets<Scalar, Index>(mesh, order);

        for (Index i = 0; i < 4; i++) {
            Index ci = mesh.get_first_corner_around_vertex(i);
            REQUIRE(mesh.get_corner_vertex(ci) == i);
        }
        for (Index i = 0; i < 2; i++) {
            Index c_start = mesh.get_facet_corner_begin(i);
            Index c_end = mesh.get_facet_corner_end(i);
            for (Index c = c_start; c < c_end; c++) {
                REQUIRE(mesh.get_corner_facet(c) == i);
            }
        }
        lagrange::testing::check_mesh(mesh);
    }
}

TEST_CASE("permute_facets benchmark", "[surface][core][permute][!benchmark]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");
    Index num_facets = mesh.get_num_facets();
    std::vector<Index> order(num_facets);
    for (Index i = 0; i < num_facets; i++) {
        order[i] = num_facets - 1 - i;
    }

    BENCHMARK("permute_facets")
    {
        return permute_facets<Scalar, Index>(mesh, order);
    };
}

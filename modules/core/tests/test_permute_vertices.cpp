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
#include <lagrange/permute_vertices.h>
#include <lagrange/remap_vertices.h>
#include <lagrange/testing/check_mesh.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>


#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/mesh_convert.h>
    #include <lagrange/reorder_mesh_vertices.h>
#endif

#include <Eigen/Core>
#include <catch2/benchmark/catch_benchmark.hpp>

TEST_CASE("permute_vertices", "[surface][reorder_vertices]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    using RowI = Eigen::Matrix<Index, 1, 3>;

    Eigen::Matrix<Scalar, 4, 3, Eigen::RowMajor> vertices;
    vertices << 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({vertices.row(0).data(), 3});
    mesh.add_vertex({vertices.row(1).data(), 3});
    mesh.add_vertex({vertices.row(2).data(), 3});
    mesh.add_vertex({vertices.row(3).data(), 3});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(0, 2, 3);

    SECTION("identity")
    {
        std::vector<Index> order{0, 1, 2, 3};
        permute_vertices<Scalar, Index>(mesh, order);
        REQUIRE(mesh.get_num_vertices() == 4);

        auto new_vertices = vertex_view(mesh);
        auto new_facets = facet_view(mesh);
        REQUIRE((new_vertices.array() == vertices.array()).all());
        REQUIRE((new_facets.row(0).array() == RowI(0, 1, 2).array()).all());
        REQUIRE((new_facets.row(1).array() == RowI(0, 2, 3).array()).all());
        lagrange::testing::check_mesh(mesh);
    }

    SECTION("reverse")
    {
        std::vector<Index> order{3, 2, 1, 0};
        permute_vertices<Scalar, Index>(mesh, order);
        REQUIRE(mesh.get_num_vertices() == 4);

        auto new_vertices = vertex_view(mesh);
        auto new_facets = facet_view(mesh);
        REQUIRE((new_vertices.row(0).array() == vertices.row(3).array()).all());
        REQUIRE((new_vertices.row(1).array() == vertices.row(2).array()).all());
        REQUIRE((new_vertices.row(2).array() == vertices.row(1).array()).all());
        REQUIRE((new_vertices.row(3).array() == vertices.row(0).array()).all());
        REQUIRE((new_facets.row(0).array() == RowI(3, 2, 1).array()).all());
        REQUIRE((new_facets.row(1).array() == RowI(3, 1, 0).array()).all());
        lagrange::testing::check_mesh(mesh);
    }

    SECTION("with attributes")
    {
        auto id = mesh.template create_attribute<int>(
            "vertex_index",
            AttributeElement::Vertex,
            AttributeUsage::Scalar);
        auto data = matrix_ref(mesh.ref_attribute<int>(id));
        data << 1, 2, 3, 4;

        std::vector<Index> order{3, 2, 1, 0};
        permute_vertices<Scalar, Index>(mesh, order);
        REQUIRE(mesh.get_num_vertices() == 4);

        const auto& attr = mesh.get_attribute<int>(id);
        REQUIRE(attr.get(0) == 4);
        REQUIRE(attr.get(1) == 3);
        REQUIRE(attr.get(2) == 2);
        REQUIRE(attr.get(3) == 1);
        lagrange::testing::check_mesh(mesh);
    }

    SECTION("with connectivity")
    {
        mesh.initialize_edges();
        std::vector<Index> order{3, 2, 1, 0};
        permute_vertices<Scalar, Index>(mesh, order);

        for (Index i = 0; i < 4; i++) {
            Index ci = mesh.get_first_corner_around_vertex(i);
            REQUIRE(mesh.get_corner_vertex(ci) == i);
        }
        lagrange::testing::check_mesh(mesh);
    }

    SECTION("invalid permuation")
    {
        std::vector<Index> order{3, 2, 1, 1000}; // Index out of bound.
        LA_REQUIRE_THROWS(permute_vertices<Scalar, Index>(mesh, order));

        order = {3, 2, 1, 1}; // Invalid permuation.
        LA_REQUIRE_THROWS(permute_vertices<Scalar, Index>(mesh, order));
    }
}

TEST_CASE("permute_vertices benchmark", "[surface][core][permute][!benchmark]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");
    Index num_vertices = mesh.get_num_vertices();
    std::vector<Index> order(num_vertices);
    for (Index i = 0; i < num_vertices; i++) {
        order[i] = num_vertices - 1 - i;
    }

    BENCHMARK("permute_vertices")
    {
        return permute_vertices<Scalar, Index>(mesh, order);
    };
    BENCHMARK("remap_vertices")
    {
        return remap_vertices<Scalar, Index>(mesh, order);
    };
#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    using VertexArray = Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
    using FacetArray = Eigen::Matrix<Index, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
    using MeshType = Mesh<VertexArray, FacetArray>;
    auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);
    BENCHMARK("reorder_mesh_vertices")
    {
        return reorder_mesh_vertices(*legacy_mesh, order);
    };
#endif
}

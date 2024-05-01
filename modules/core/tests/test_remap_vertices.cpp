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

#include <lagrange/testing/common.h>
#include <catch2/catch_approx.hpp>

#include <lagrange/Attribute.h>
#include <lagrange/Logger.h>
#include <lagrange/mesh_cleanup/remove_duplicate_facets.h>
#include <lagrange/remap_vertices.h>
#include <lagrange/views.h>

#include <Eigen/Core>

TEST_CASE("remap_vertices", "[core][surface][utilities]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({0, 0, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(2, 1, 3);

    const Eigen::Matrix<Scalar, Eigen::Dynamic, 3, Eigen::RowMajor> input_vertices =
        vertex_view(mesh);
    const Eigen::Matrix<Index, Eigen::Dynamic, 3, Eigen::RowMajor> input_facets = facet_view(mesh);

    SECTION("All collapse to one")
    {
        std::vector<Index> old_to_new{0, 0, 0, 0};

        SECTION("mixed")
        {
            remap_vertices<Scalar, Index>(mesh, old_to_new);

            auto vertices = vertex_view(mesh);
            REQUIRE(vertices(0) == Catch::Approx(0.25));
            REQUIRE(vertices(1) == Catch::Approx(0.25));
            REQUIRE(vertices(2) == Catch::Approx(0));
        }

        SECTION("keep first")
        {
            RemapVerticesOptions options;
            options.collision_policy_float = MappingPolicy::KeepFirst;
            remap_vertices<Scalar, Index>(mesh, old_to_new, options);

            auto vertices = vertex_view(mesh);
            REQUIRE(vertices(0) == Catch::Approx(0));
            REQUIRE(vertices(1) == Catch::Approx(0));
            REQUIRE(vertices(2) == Catch::Approx(0));
        }

        SECTION("average")
        {
            RemapVerticesOptions options;
            options.collision_policy_float = MappingPolicy::Average;
            remap_vertices<Scalar, Index>(mesh, old_to_new, options);

            auto vertices = vertex_view(mesh);
            REQUIRE(vertices(0) == Catch::Approx(0.25));
            REQUIRE(vertices(1) == Catch::Approx(0.25));
            REQUIRE(vertices(2) == Catch::Approx(0));
        }

        REQUIRE(mesh.get_num_vertices() == 1);
        REQUIRE(mesh.get_num_facets() == 2);

        auto facets = facet_view(mesh);
        REQUIRE(facets.maxCoeff() == 0);
        REQUIRE(facets.minCoeff() == 0);
    }

    SECTION("Nothing should happen")
    {
        std::vector<Index> old_to_new{0, 1, 2, 3};
        remap_vertices<Scalar, Index>(mesh, old_to_new);
        REQUIRE(mesh.get_num_vertices() == 4);
        REQUIRE(mesh.get_num_facets() == 2);

        auto vertices = vertex_view(mesh);
        REQUIRE(vertices == input_vertices);
        auto facets = facet_view(mesh);
        REQUIRE(facets == input_facets);
    }

    SECTION("Only two points should remain")
    {
        std::vector<Index> old_to_new = {1, 1, 0, 0};
        remap_vertices<Scalar, Index>(mesh, old_to_new);
        REQUIRE(mesh.get_num_vertices() == 2);
        REQUIRE(mesh.get_num_facets() == 2);

        auto vertices = vertex_view(mesh);
        REQUIRE(vertices(0, 0) == Catch::Approx(0));
        REQUIRE(vertices(0, 1) == Catch::Approx(0.5));
        REQUIRE(vertices(0, 2) == Catch::Approx(0));
        REQUIRE(vertices(1, 0) == Catch::Approx(0.5));
        REQUIRE(vertices(1, 1) == Catch::Approx(0));
        REQUIRE(vertices(1, 2) == Catch::Approx(0));

        auto facets = facet_view(mesh);
        REQUIRE(facets(0, 0) == 1);
        REQUIRE(facets(0, 1) == 1);
        REQUIRE(facets(0, 2) == 0);
        REQUIRE(facets(1, 0) == 0);
        REQUIRE(facets(1, 1) == 1);
        REQUIRE(facets(1, 2) == 0);
    }

    SECTION("with edges")
    {
        ScopedLogLevel guard(spdlog::level::err);
        mesh.initialize_edges();

        std::vector<Index> old_to_new{0, 2, 1, 0};
        remap_vertices<Scalar, Index>(mesh, old_to_new);
    }

    SECTION("Invalid ordering")
    {
        std::vector<Index> old_to_new = {3, 3, 2, 2}; // non surjective.
        LA_REQUIRE_THROWS(remap_vertices<Scalar, Index>(mesh, old_to_new));

        old_to_new = {0, 1, 2, 7}; // Exceeding bound.
        LA_REQUIRE_THROWS(remap_vertices<Scalar, Index>(mesh, old_to_new));
    }

    SECTION("Vertex attribute")
    {
        std::array<Index, 4> indices{0, 1, 2, 3};
        auto id = mesh.create_attribute<Index>(
            "vertex_index",
            AttributeElement::Vertex,
            AttributeUsage::VertexIndex,
            1,
            indices);

        std::vector<Index> old_to_new{0, 0, 0, 0};

        SECTION("mixed")
        {
            RemapVerticesOptions options;
            options.collision_policy_float = MappingPolicy::Average;
            options.collision_policy_integral = MappingPolicy::KeepFirst;
            remap_vertices<Scalar, Index>(mesh, old_to_new, options);

            auto& attr = mesh.get_attribute<Index>(id);
            REQUIRE(attr.get_num_elements() == 1);
            REQUIRE(attr.get(0) == 0);
        }

        SECTION("keep first")
        {
            RemapVerticesOptions options;
            options.collision_policy_float = MappingPolicy::KeepFirst;
            options.collision_policy_integral = MappingPolicy::KeepFirst;
            remap_vertices<Scalar, Index>(mesh, old_to_new, options);

            auto& attr = mesh.get_attribute<Index>(id);
            REQUIRE(attr.get_num_elements() == 1);
            REQUIRE(attr.get(0) == 0);
        }

        SECTION("average")
        {
            RemapVerticesOptions options;
            options.collision_policy_float = MappingPolicy::Average;
            options.collision_policy_integral = MappingPolicy::Average;
            LA_REQUIRE_THROWS(remap_vertices<Scalar, Index>(mesh, old_to_new, options));
        }

        SECTION("error")
        {
            RemapVerticesOptions options;
            options.collision_policy_float = MappingPolicy::Error;
            options.collision_policy_integral = MappingPolicy::Error;
            LA_REQUIRE_THROWS(remap_vertices<Scalar, Index>(mesh, old_to_new, options));

            old_to_new = {3, 2, 1, 0};
            remap_vertices<Scalar, Index>(mesh, old_to_new, options);

            auto& attr = mesh.get_attribute<Index>(id);
            REQUIRE(attr.get_num_elements() == 4);
            REQUIRE(attr.get(0) == 0);
            REQUIRE(attr.get(1) == 1);
            REQUIRE(attr.get(2) == 2);
            REQUIRE(attr.get(3) == 3);
        }
    }
}

TEST_CASE("remap_vertices 6 vtx", "[core][surface][utilities]")
{
    using Scalar = double;
    using Index = uint32_t;

    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(3, 4, 5);

    SECTION("merge one side - no edges")
    {
        std::vector<Index> old_to_new{0, 1, 2, 1, 2, 3};
        {
            auto copy = mesh;
            copy.initialize_edges();
            REQUIRE(copy.get_num_edges() == 6);
        }
        lagrange::remap_vertices<Scalar, Index>(mesh, old_to_new);
        mesh.initialize_edges();
        REQUIRE(mesh.get_num_vertices() == 4);
        REQUIRE(mesh.get_num_edges() == 5);
        REQUIRE(mesh.get_num_facets() == 2);
    }

    SECTION("merge one side - with edges")
    {
        std::vector<Index> old_to_new{0, 1, 2, 1, 2, 3};
        // clang-format off
        mesh.initialize_edges(std::vector<Index>{
            0, 1,
            1, 2,
            2, 0,
            3, 4,
            4, 5,
            5, 3,
        });
        // clang-format on
        auto edge_index_id = mesh.create_attribute<Index>(
            "edge_index",
            lagrange::AttributeElement::Edge,
            lagrange::AttributeUsage::EdgeIndex,
            1,
            std::vector<Index>{0, 1, 2, 3, 4, 5});
        auto edge_scalar_id = mesh.create_attribute<Index>(
            "edge_scalar",
            lagrange::AttributeElement::Edge,
            lagrange::AttributeUsage::Scalar,
            1,
            std::vector<Index>{0, 1, 2, 3, 4, 5});
        auto v2e_index_id = mesh.create_attribute<Index>(
            "v2e_index",
            lagrange::AttributeElement::Vertex,
            lagrange::AttributeUsage::EdgeIndex,
            1,
            std::vector<Index>{0, 1, 2, 3, 4, 5});
        auto v2e_scalar_id = mesh.create_attribute<Index>(
            "v2e_scalar",
            lagrange::AttributeElement::Vertex,
            lagrange::AttributeUsage::Scalar,
            1,
            std::vector<Index>{0, 1, 2, 3, 4, 5});
        lagrange::remap_vertices<Scalar, Index>(mesh, old_to_new);
        REQUIRE(mesh.get_num_vertices() == 4);
        REQUIRE(mesh.get_num_edges() == 5);
        REQUIRE(mesh.get_num_facets() == 2);
        auto edge_index = lagrange::attribute_vector_view<Index>(mesh, edge_index_id);
        auto edge_scalar = lagrange::attribute_vector_view<Index>(mesh, edge_scalar_id);
        auto v2e_index = lagrange::attribute_vector_view<Index>(mesh, v2e_index_id);
        auto v2e_scalar = lagrange::attribute_vector_view<Index>(mesh, v2e_scalar_id);
        Eigen::VectorX<Index> expected_edge_index(5);
        Eigen::VectorX<Index> expected_edge_scalar(5);
        Eigen::VectorX<Index> expected_v2e_index(4);
        Eigen::VectorX<Index> expected_v2e_scalar(4);
        expected_edge_index << 0, 1, 2, 3, 4;
        expected_edge_scalar << 0, 2, 1, 5, 4;
        expected_v2e_index << 0, 2, 1, 3;
        expected_v2e_scalar << 0, 1, 2, 5;
        REQUIRE(edge_index == expected_edge_index);
        REQUIRE(edge_scalar == expected_edge_scalar);
        REQUIRE(v2e_index == expected_v2e_index);
        REQUIRE(v2e_scalar == expected_v2e_scalar);
    }

    SECTION("merge two tris - no edges")
    {
        std::vector<Index> old_to_new{0, 1, 2, 1, 2, 0};
        {
            auto copy = mesh;
            copy.initialize_edges();
            REQUIRE(copy.get_num_edges() == 6);
        }
        lagrange::remap_vertices<Scalar, Index>(mesh, old_to_new);
        mesh.initialize_edges();
        REQUIRE(mesh.get_num_vertices() == 3);
        REQUIRE(mesh.get_num_edges() == 3);
        REQUIRE(mesh.get_num_facets() == 2); // duplicate facet!
        lagrange::remove_duplicate_facets(mesh);
        REQUIRE(mesh.get_num_facets() == 1);
        auto V = vertex_view(mesh);
        REQUIRE(V(0, 0) == 0.5);
        REQUIRE(V(0, 1) == 0.5);
    }

    SECTION("merge two tris - with edges")
    {
        std::vector<Index> old_to_new{0, 1, 2, 1, 2, 0};
        // clang-format off
        mesh.initialize_edges(std::vector<Index>{
            0, 1,
            1, 2,
            2, 0,
            3, 4,
            4, 5,
            5, 3,
        });
        // clang-format on
        auto edge_index_id = mesh.create_attribute<Index>(
            "edge_index",
            lagrange::AttributeElement::Edge,
            lagrange::AttributeUsage::EdgeIndex,
            1,
            std::vector<Index>{0, 1, 2, 3, 4, 5});
        auto edge_scalar_id = mesh.create_attribute<Index>(
            "edge_scalar",
            lagrange::AttributeElement::Edge,
            lagrange::AttributeUsage::Scalar,
            1,
            std::vector<Index>{0, 1, 2, 3, 4, 5});
        auto v2e_index_id = mesh.create_attribute<Index>(
            "v2e_index",
            lagrange::AttributeElement::Vertex,
            lagrange::AttributeUsage::EdgeIndex,
            1,
            std::vector<Index>{0, 1, 2, 3, 4, 5});
        auto v2e_scalar_id = mesh.create_attribute<Index>(
            "v2e_scalar",
            lagrange::AttributeElement::Vertex,
            lagrange::AttributeUsage::Scalar,
            1,
            std::vector<Index>{0, 1, 2, 3, 4, 5});
        lagrange::remap_vertices<Scalar, Index>(mesh, old_to_new);
        REQUIRE(mesh.get_num_vertices() == 3);
        REQUIRE(mesh.get_num_edges() == 3);
        REQUIRE(mesh.get_num_facets() == 2); // duplicate facet!
        auto edge_index = lagrange::attribute_vector_view<Index>(mesh, edge_index_id);
        auto edge_scalar = lagrange::attribute_vector_view<Index>(mesh, edge_scalar_id);
        auto v2e_index = lagrange::attribute_vector_view<Index>(mesh, v2e_index_id);
        auto v2e_scalar = lagrange::attribute_vector_view<Index>(mesh, v2e_scalar_id);
        Eigen::VectorX<Index> expected_edge_index(3);
        Eigen::VectorX<Index> expected_edge_scalar(3);
        Eigen::VectorX<Index> expected_v2e_index(3);
        Eigen::VectorX<Index> expected_v2e_scalar(3);
        expected_edge_index << 0, 1, 2;
        expected_edge_scalar << 0, 2, 1;
        expected_v2e_index << 0, 2, 1;
        expected_v2e_scalar << 0, 1, 2;
        REQUIRE(edge_index == expected_edge_index);
        REQUIRE(edge_scalar == expected_edge_scalar);
        REQUIRE(v2e_index == expected_v2e_index);
        REQUIRE(v2e_scalar == expected_v2e_scalar);
    }
}

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
        LA_REQUIRE_THROWS(remap_vertices<Scalar, Index>(mesh, old_to_new));
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
            REQUIRE(attr.get(0) == 3);
            REQUIRE(attr.get(1) == 2);
            REQUIRE(attr.get(2) == 1);
            REQUIRE(attr.get(3) == 0);
        }
    }
}

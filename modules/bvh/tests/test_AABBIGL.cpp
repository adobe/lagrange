/*
 * Copyright 2019 Adobe. All rights reserved.
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

#include <limits>

#include <lagrange/bvh/AABBIGL.h>
#include <lagrange/bvh/create_BVH.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/utils/range.h>

TEST_CASE("AABBIGL", "[bvh][igl][aabb]")
{
    using namespace lagrange;

    SECTION("Simple")
    {
        using MeshType = TriangleMesh3D;
        using VertexArray = typename MeshType::VertexArray;
        using FacetArray = typename MeshType::FacetArray;

        VertexArray vertices(3, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;
        FacetArray facets(1, 3);
        facets << 0, 1, 2;

        auto mesh = lagrange::wrap_with_mesh(vertices, facets);
        auto engine = lagrange::bvh::create_BVH(lagrange::bvh::BVHType::IGL, *mesh);

        SECTION("Query on input point")
        {
            for (const auto p : vertices.rowwise()) {
                auto r = engine->query_closest_point(p);
                REQUIRE(r.embedding_element_idx == 0);
                REQUIRE(r.closest_vertex_idx >= 0);
                REQUIRE(r.closest_vertex_idx < vertices.rows());
                REQUIRE(r.closest_point == vertices.row(r.closest_vertex_idx));
                REQUIRE(r.squared_distance == Catch::Approx(0.0));
            }
        }

        SECTION("Batch query")
        {
            auto r = engine->batch_query_closest_point(vertices);
            REQUIRE(r.size() == 3);

            REQUIRE(r[0].embedding_element_idx == 0);
            REQUIRE(r[1].embedding_element_idx == 0);
            REQUIRE(r[2].embedding_element_idx == 0);

            REQUIRE(r[0].closest_vertex_idx == 0);
            REQUIRE(r[1].closest_vertex_idx == 1);
            REQUIRE(r[2].closest_vertex_idx == 2);

            REQUIRE(r[0].closest_point == vertices.row(0));
            REQUIRE(r[1].closest_point == vertices.row(1));
            REQUIRE(r[2].closest_point == vertices.row(2));

            REQUIRE(r[0].squared_distance == Catch::Approx(0.0));
            REQUIRE(r[1].squared_distance == Catch::Approx(0.0));
            REQUIRE(r[2].squared_distance == Catch::Approx(0.0));
        }
    }

    SECTION("cube")
    {
        using MeshType = TriangleMesh3D;
        using VertexArray = typename MeshType::VertexArray;
        using FacetArray = typename MeshType::FacetArray;

        VertexArray vertices(8, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0, 1.0,
            0.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0;
        FacetArray facets(12, 3);
        facets << 0, 2, 1, 1, 2, 3, 4, 5, 6, 6, 5, 7, 1, 3, 5, 5, 3, 7, 0, 4, 2, 2, 4, 6, 0, 1, 5,
            0, 5, 4, 2, 6, 7, 2, 7, 3;

        auto engine = lagrange::bvh::create_BVH(lagrange::bvh::BVHType::IGL, vertices, facets);
        SECTION("Query on input point")
        {
            for (const auto p : vertices.rowwise()) {
                auto r = engine->query_closest_point(p);
                REQUIRE(r.closest_vertex_idx >= 0);
                REQUIRE(r.closest_vertex_idx < vertices.rows());
                REQUIRE(r.closest_point == vertices.row(r.closest_vertex_idx));
                REQUIRE(r.squared_distance == Catch::Approx(0.0));
            }
        }

        SECTION("Facet centers")
        {
            auto mesh = lagrange::wrap_with_mesh(vertices, facets);
            const auto num_facets = mesh->get_num_facets();
            for (auto i : range(num_facets)) {
                auto centroid = (vertices.row(facets(i, 0)) + vertices.row(facets(i, 1)) +
                                 vertices.row(facets(i, 2))) /
                                3.0;
                auto r = engine->query_closest_point(centroid);
                REQUIRE(r.embedding_element_idx == i);
                REQUIRE((r.closest_point - centroid).norm() == Catch::Approx(0.0).margin(1e-12));
                REQUIRE(r.squared_distance == Catch::Approx(0.0).margin(1e-12));
            }
        }
    }
}

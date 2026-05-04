/*
 * Copyright 2026 Adobe. All rights reserved.
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

#include <lagrange/compute_facet_facet_adjacency.h>

#include <set>

TEST_CASE("compute_facet_facet_adjacency", "[surface][adjacency]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SECTION("single triangle")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);

        auto adj = compute_facet_facet_adjacency(mesh);
        REQUIRE(adj.get_num_entries() == 1);
        REQUIRE(adj.get_neighbors(0).size() == 0);
    }

    SECTION("two triangles sharing an edge")
    {
        //  2
        // / \.
        // 0--1
        // \ /
        //  3
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0.5, 1, 0});
        mesh.add_vertex({0.5, -1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 0, 3);

        auto adj = compute_facet_facet_adjacency(mesh);
        REQUIRE(adj.get_num_entries() == 2);

        auto n0 = adj.get_neighbors(0);
        auto n1 = adj.get_neighbors(1);
        REQUIRE(n0.size() == 1);
        REQUIRE(n1.size() == 1);
        REQUIRE(n0[0] == 1);
        REQUIRE(n1[0] == 0);
    }

    SECTION("triangle strip")
    {
        // 0--2--4
        // |\ |\ |
        // | \| \|
        // 1--3--5
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({2, 1, 0});
        mesh.add_vertex({2, 0, 0});
        mesh.add_triangle(0, 1, 3); // f0
        mesh.add_triangle(0, 3, 2); // f1
        mesh.add_triangle(2, 3, 5); // f2
        mesh.add_triangle(2, 5, 4); // f3

        auto adj = compute_facet_facet_adjacency(mesh);
        REQUIRE(adj.get_num_entries() == 4);

        // f0 shares edge (0,3) with f1, edge (1,3) with nobody
        // f1 shares edge (0,3) with f0, edge (2,3) with f2
        // f2 shares edge (2,3) with f1, edge (3,5) with nobody, edge (2,5) with f3
        // f3 shares edge (2,5) with f2

        auto n0 = adj.get_neighbors(0);
        auto n1 = adj.get_neighbors(1);
        auto n2 = adj.get_neighbors(2);
        auto n3 = adj.get_neighbors(3);

        REQUIRE(n0.size() == 1);
        CHECK(n0[0] == 1);

        REQUIRE(n1.size() == 2);
        std::set<Index> s1(n1.begin(), n1.end());
        CHECK(s1.count(0) == 1);
        CHECK(s1.count(2) == 1);

        REQUIRE(n2.size() == 2);
        std::set<Index> s2(n2.begin(), n2.end());
        CHECK(s2.count(1) == 1);
        CHECK(s2.count(3) == 1);

        REQUIRE(n3.size() == 1);
        CHECK(n3[0] == 2);
    }

    SECTION("isolated triangles")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({2, 0, 0});
        mesh.add_vertex({3, 0, 0});
        mesh.add_vertex({2, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(3, 4, 5);

        auto adj = compute_facet_facet_adjacency(mesh);
        REQUIRE(adj.get_num_entries() == 2);
        REQUIRE(adj.get_neighbors(0).size() == 0);
        REQUIRE(adj.get_neighbors(1).size() == 0);
    }

    SECTION("non-manifold edge (3 facets sharing an edge)")
    {
        // Three triangles sharing edge (0,1):
        //   f0: (0, 1, 2)
        //   f1: (0, 1, 3)
        //   f2: (0, 1, 4)
        // Each facet should be adjacent to the other two.
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({0, -1, 0});
        mesh.add_vertex({0, 0, 1});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 1, 3);
        mesh.add_triangle(0, 1, 4);

        auto adj = compute_facet_facet_adjacency(mesh);
        REQUIRE(adj.get_num_entries() == 3);

        for (Index f = 0; f < 3; ++f) {
            auto neighbors = adj.get_neighbors(f);
            std::set<Index> nset(neighbors.begin(), neighbors.end());
            REQUIRE(nset.size() == 2);
            for (Index g = 0; g < 3; ++g) {
                if (g != f) {
                    CHECK(nset.count(g) == 1);
                }
            }
        }
    }

    SECTION("degenerate facet incident to same edge twice")
    {
        // A quad facet {0, 1, 2, 1} visits edge (0,1) twice via two different
        // corners. foreach_facet_around_facet() reports the neighbor once per
        // shared edge reference, so neighbors are NOT deduplicated.
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0}); // v0
        mesh.add_vertex({1, 0, 0}); // v1
        mesh.add_vertex({0, 1, 0}); // v2
        mesh.add_vertex({1, 1, 0}); // v3
        mesh.add_quad(0, 1, 2, 1); // degenerate quad (v1 appears twice)
        mesh.add_quad(0, 1, 3, 2); // normal quad sharing edge (0,1)

        auto adj = compute_facet_facet_adjacency(mesh);
        REQUIRE(adj.get_num_entries() == 2);

        // f0 ({0,1,2,1}) references edge (0,1) from two corners, so f1 appears
        // twice in f0's neighbor list. f1 ({0,1,3,2}) references edge (0,1) once,
        // but foreach_facet_around_edge reports f0 twice (once per corner of f0 on
        // that edge), so f0 also appears twice in f1's neighbor list.
        // No self-neighbors should exist.
        auto n0 = adj.get_neighbors(0);
        auto n1 = adj.get_neighbors(1);
        REQUIRE(n0.size() == 2);
        CHECK(n0[0] == 1);
        CHECK(n0[1] == 1);
        REQUIRE(n1.size() == 2);
        CHECK(n1[0] == 0);
        CHECK(n1[1] == 0);
    }

    SECTION("edges already initialized")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0.5, 1, 0});
        mesh.add_vertex({0.5, -1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 0, 3);
        mesh.initialize_edges();

        auto adj = compute_facet_facet_adjacency(mesh);
        auto n0 = adj.get_neighbors(0);
        REQUIRE(n0.size() == 1);
        REQUIRE(n0[0] == 1);
    }
}

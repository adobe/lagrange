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
#include <Eigen/Core>
#include <lagrange/testing/common.h>
#include <iostream>

#include <lagrange/Connectivity.h>
#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>

TEST_CASE("ConnectivitySimpleTriangleMesh", "[connectivity][triangle_mesh][simple]")
{
    using namespace lagrange;

    Vertices3D vertices(3, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;
    Triangles facets(1, 3);
    facets << 0, 1, 2;

    auto mesh = create_mesh(vertices, facets);
    mesh->initialize_connectivity();

    SECTION("Vertex-vertex adjacency")
    {
        for (int i = 0; i < 3; i++) {
            const auto& adj_v = mesh->get_vertices_adjacent_to_vertex(i);
            REQUIRE(adj_v.size() == 2);
            REQUIRE(adj_v[0] != adj_v[1]);
            REQUIRE(adj_v[0] != i);
            REQUIRE(adj_v[1] != i);
        }
    }

    SECTION("Vertex-facet adjacency")
    {
        for (int i = 0; i < 3; i++) {
            const auto& adj_f = mesh->get_facets_adjacent_to_vertex(i);
            REQUIRE(adj_f.size() == 1);
            REQUIRE(adj_f[0] == 0);
        }
    }

    SECTION("Facet-facet adjacency")
    {
        const auto& adj_f = mesh->get_facets_adjacent_to_facet(0);
        REQUIRE(adj_f.empty());
    }
}

TEST_CASE("ConnectivityTriangleMesh", "[connectivity][triangle_mesh][simple]")
{
    using namespace lagrange;

    Vertices3D vertices(4, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0;
    Triangles facets(4, 3);
    facets << 0, 2, 1, 0, 1, 3, 1, 2, 3, 2, 0, 3;

    auto mesh = create_mesh(vertices, facets);
    mesh->initialize_connectivity();

    using Index = decltype(mesh)::element_type::Index;

    SECTION("Vertex-vertex adjacency")
    {
        for (Index i = 0; i < 4; i++) {
            const auto& adj_v = mesh->get_vertices_adjacent_to_vertex(i);
            REQUIRE(adj_v.size() == 3);
        }
        for (Index i = 0; i < 4; i++) {
            for (Index j = 0; j < 3; j++) {
                const Index curr_v = facets(i, j);
                const Index next_v = facets(i, (j + 1) % 3);
                const auto& adj_curr_v = mesh->get_vertices_adjacent_to_vertex(curr_v);
                const auto next_itr = std::find(adj_curr_v.begin(), adj_curr_v.end(), next_v);
                REQUIRE(next_itr != adj_curr_v.end());

                const auto& adj_next_v = mesh->get_vertices_adjacent_to_vertex(next_v);
                const auto curr_itr = std::find(adj_next_v.begin(), adj_next_v.end(), curr_v);
                REQUIRE(curr_itr != adj_next_v.end());
            }
        }
    }

    SECTION("Vertex-facet adjacency")
    {
        for (Index i = 0; i < 4; i++) {
            const auto& adj_f = mesh->get_facets_adjacent_to_vertex(i);
            REQUIRE(adj_f.size() == 3);
        }

        for (Index i = 0; i < 4; i++) {
            for (Index j = 0; j < 3; j++) {
                const Index v = facets(i, j);
                const auto& adj_f = mesh->get_facets_adjacent_to_vertex(v);
                const auto f_itr = std::find(adj_f.begin(), adj_f.end(), i);
                REQUIRE(f_itr != adj_f.end());
            }
        }
    }

    SECTION("Facet-facet adjacency")
    {
        for (Index i = 0; i < 4; i++) {
            const auto& adj_f = mesh->get_facets_adjacent_to_facet(i);
            REQUIRE(adj_f.size() == 3);
            const auto& self_itr = std::find(adj_f.begin(), adj_f.end(), i);
            REQUIRE(self_itr == adj_f.end());
        }
    }
}

TEST_CASE("ConnectivityQuadMesh", "[connectivity][quad_mesh][simple]")
{
    using namespace lagrange;

    Vertices3D vertices(8, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0,
        1.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0;
    Quads facets(6, 4);
    facets << 0, 2, 3, 1, 4, 5, 6, 7, 4, 0, 1, 5, 2, 7, 6, 3, 6, 5, 1, 3, 4, 7, 2, 0;

    auto mesh = create_mesh(vertices, facets);
    mesh->initialize_connectivity();

    using Index = decltype(mesh)::element_type::Index;

    SECTION("Vertex-vertex adjacency")
    {
        for (Index i = 0; i < 8; i++) {
            const auto& v_adj = mesh->get_vertices_adjacent_to_vertex(i);
            REQUIRE(v_adj.size() == 3);
        }
        for (Index i = 0; i < 6; i++) {
            for (Index j = 0; j < 4; j++) {
                const Index curr_v = facets(i, j);
                const Index next_v = facets(i, (j + 1) % 4);
                const auto& adj_curr_v = mesh->get_vertices_adjacent_to_vertex(curr_v);
                const auto next_itr = std::find(adj_curr_v.begin(), adj_curr_v.end(), next_v);
                REQUIRE(next_itr != adj_curr_v.end());

                const auto& adj_next_v = mesh->get_vertices_adjacent_to_vertex(next_v);
                const auto curr_itr = std::find(adj_next_v.begin(), adj_next_v.end(), curr_v);
                REQUIRE(curr_itr != adj_next_v.end());
            }
        }
    }

    SECTION("Vertex-facet adjacency")
    {
        for (Index i = 0; i < 8; i++) {
            const auto& adj_f = mesh->get_facets_adjacent_to_vertex(i);
            REQUIRE(adj_f.size() == 3);
        }

        for (Index i = 0; i < 6; i++) {
            for (Index j = 0; j < 4; j++) {
                const Index v = facets(i, j);
                const auto& adj_f = mesh->get_facets_adjacent_to_vertex(v);
                const auto f_itr = std::find(adj_f.begin(), adj_f.end(), i);
                REQUIRE(f_itr != adj_f.end());
            }
        }
    }

    SECTION("Facet-facet adjacency")
    {
        for (Index i = 0; i < 6; i++) {
            const auto& adj_f = mesh->get_facets_adjacent_to_facet(i);
            REQUIRE(adj_f.size() == 4);
            const auto& self_itr = std::find(adj_f.begin(), adj_f.end(), i);
            REQUIRE(self_itr == adj_f.end());
        }
    }
}

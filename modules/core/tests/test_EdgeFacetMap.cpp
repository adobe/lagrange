/*
 * Copyright 2020 Adobe. All rights reserved.
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

#include <cmath>

#include <lagrange/Edge.h>
#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>

namespace {

// Computes a mapping from each edge to its adjacent facets.
template <typename MeshType>
lagrange::EdgeFacetMap<MeshType> compute_edge_facet_map(const MeshType& mesh)
{
    using namespace lagrange;
    using Index = typename MeshType::Index;
    std::unordered_map<EdgeType<Index>, std::vector<Index>> edge_facet_map;
    const Index num_facets = mesh.get_num_facets();
    const Index vertex_per_facet = mesh.get_vertex_per_facet();
    const typename MeshType::FacetArray& facets = mesh.get_facets();
    for (Index i = 0; i < num_facets; ++i) {
        for (Index j = 0; j < vertex_per_facet; ++j) {
            Index v1 = facets(i, j);
            Index v2 = facets(i, (j + 1) % vertex_per_facet);
            EdgeType<Index> edge(v1, v2);
            auto it = edge_facet_map.find(edge);
            if (it == edge_facet_map.end()) {
                edge_facet_map[edge] = {i};
            } else {
                it->second.push_back(i);
            }
        }
    }
    return edge_facet_map;
}

} // namespace


TEST_CASE("EdgeFacetMap", "[EdgeFacetMap]")
{
    using namespace lagrange;
    Vertices3D vertices(3, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;
    Triangles facets(1, 3);
    facets << 0, 1, 2;

    const auto mesh = create_mesh(vertices, facets);
    using Index = decltype(mesh)::element_type::Index;

    auto edge_map = ::compute_edge_facet_map(*mesh);

    REQUIRE(edge_map.size() == 3);
    for (const auto& item : edge_map) {
        REQUIRE(item.second.size() == 1);
        REQUIRE(item.second[0] == 0);
    }

    REQUIRE(edge_map.find({{0, 0}}) == edge_map.end());
    REQUIRE(edge_map.find({{10, -1}}) == edge_map.end());
    REQUIRE(edge_map.find({{0, 1}}) != edge_map.end());
    REQUIRE(edge_map.find({{1, 2}}) != edge_map.end());
    REQUIRE(edge_map.find({{2, 0}}) != edge_map.end());

    SECTION("Update mesh")
    {
        vertices.conservativeResize(4, 3);
        vertices.row(3) << 0.0, 0.0, 1.0;
        facets.resize(4, 3);
        facets << 0, 2, 1, 3, 0, 1, 1, 2, 3, 3, 2, 0;
        mesh->import_vertices(vertices);
        mesh->import_facets(facets);
        REQUIRE(mesh->get_num_vertices() == 4);
        REQUIRE(mesh->get_num_facets() == 4);

        edge_map = ::compute_edge_facet_map(*mesh);

        REQUIRE(edge_map.size() == 6);
        size_t counts[4] = {0, 0, 0, 0};
        for (const auto& item : edge_map) {
            REQUIRE(item.second.size() == 2);
            counts[item.second[0]]++;
            counts[item.second[1]]++;
        }
        REQUIRE(counts[0] == 3);
        REQUIRE(counts[1] == 3);
        REQUIRE(counts[2] == 3);
        REQUIRE(counts[3] == 3);
    }

    SECTION("Non-manifold")
    {
        vertices.conservativeResize(5, 3);
        vertices.row(3) << 0.0, 0.0, 1.0;
        vertices.row(4) << 0.5, 0.5, 0.5;
        facets.resize(3, 3);
        facets << 0, 1, 2, 0, 1, 3, 0, 1, 4;
        mesh->import_vertices(vertices);
        mesh->import_facets(facets);
        REQUIRE(mesh->get_num_vertices() == 5);
        REQUIRE(mesh->get_num_facets() == 3);

        edge_map = ::compute_edge_facet_map(*mesh);

        REQUIRE(edge_map.size() == 7);
        auto itr = edge_map.find({{0, 1}});
        REQUIRE(itr != edge_map.end());
        REQUIRE(itr->second.size() == 3);
    }

    SECTION("Fan")
    {
        constexpr Index N = 10;
        vertices.resize(N + 1, 3);
        vertices.row(0).setZero();
        for (Index i = 1; i < N + 1; i++) {
            const double ratio = double(i - 1) / double(N);
            const double angle = M_PI * 2 * ratio;
            vertices.row(i) << cos(angle), sin(angle), 0.0;
        }

        facets.resize(N, 3);
        for (Index i = 1; i <= N; i++) {
            const Index curr = i;
            const Index next = i % N + 1;
            facets.row(i - 1) << 0, curr, next;
        }

        mesh->import_vertices(vertices);
        mesh->import_facets(facets);
        REQUIRE(mesh->get_num_vertices() == N + 1);
        REQUIRE(mesh->get_num_facets() == N);

        edge_map = ::compute_edge_facet_map(*mesh);

        REQUIRE(edge_map.size() == 2 * N);

        std::unordered_set<decltype(mesh)::element_type::Index> active_facets;
        SECTION("Activate disconnected facets")
        {
            active_facets.clear();
            for (Index i = 0; i < N; i += 2) {
                active_facets.insert(i);
            }
            edge_map = compute_edge_facet_map_in_active_facets(*mesh, active_facets);
            REQUIRE(edge_map.size() == N / 2 * 3);
            for (const auto& item : edge_map) {
                REQUIRE(item.second.size() == 1);
            }
        }

        SECTION("Activate connected facets")
        {
            active_facets.clear();
            for (Index i = 0; i < N / 2; i++) {
                active_facets.insert(i);
            }
            edge_map = compute_edge_facet_map_in_active_facets(*mesh, active_facets);
            REQUIRE(edge_map.size() == N + 1);
        }

        SECTION("Activate center vertex")
        {
            edge_map = compute_edge_facet_map_in_active_vertices(*mesh, {0});
            REQUIRE(edge_map.size() == 2 * N);
        }

        SECTION("Activate boundary vertex")
        {
            edge_map = compute_edge_facet_map_in_active_vertices(*mesh, {1});
            REQUIRE(edge_map.size() == 5);
        }
    }
}

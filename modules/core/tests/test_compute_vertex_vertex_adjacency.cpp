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
#include <lagrange/testing/common.h>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <lagrange/compute_vertex_vertex_adjacency.h>
#include <lagrange/utils/span.h>

#include <set>

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/Mesh.h>
    #include <lagrange/mesh_convert.h>
#endif

TEST_CASE("compute_vertex_vertex_adjacency", "[surface][adjacency][utilities]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto check_adjacency = [](SurfaceMesh<Scalar, Index>& mesh,
                              const AdjacencyList<Index>& adjacency_list) {
        mesh.initialize_edges();
        for (Index ei = 0; ei < mesh.get_num_edges(); ei++) {
            const auto e_vertices = mesh.get_edge_vertices(ei);
            const Index v0 = e_vertices[0];
            const Index v1 = e_vertices[1];

            const auto v0_adj = adjacency_list.get_neighbors(v0);
            const auto v1_adj = adjacency_list.get_neighbors(v1);

            REQUIRE(std::find(v0_adj.begin(), v0_adj.end(), v1) != v0_adj.end());
            REQUIRE(std::find(v1_adj.begin(), v1_adj.end(), v0) != v1_adj.end());
        }
    };

    SECTION("single triangle")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);

        auto adjacency_list = compute_vertex_vertex_adjacency(mesh);
        REQUIRE(adjacency_list.get_num_entries() == 3);
        check_adjacency(mesh, adjacency_list);
    }

    SECTION("single quad")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_quad(0, 1, 3, 2);

        auto adjacency_list = compute_vertex_vertex_adjacency(mesh);
        REQUIRE(adjacency_list.get_num_entries() == 4);
        check_adjacency(mesh, adjacency_list);
    }

    SECTION("2 triangles")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(2, 1, 3);

        auto adjacency_list = compute_vertex_vertex_adjacency(mesh);
        check_adjacency(mesh, adjacency_list);
    }

    SECTION("quad + tri")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_vertex({2, 0, 0});
        mesh.add_quad(0, 1, 3, 2);
        mesh.add_triangle(3, 1, 4);

        auto adjacency_list = compute_vertex_vertex_adjacency(mesh);
        check_adjacency(mesh, adjacency_list);
    }
}

TEST_CASE("compute_vertex_vertex_adjacency - facet connectivity", "[surface][adjacency][utilities]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SECTION("single triangle - facet same as edge")
    {
        // Triangle: all 3 pairs are both edges and facet-pairs, so results should match.
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);

        auto adj_edge = compute_vertex_vertex_adjacency(mesh, DualConnectivityType::Edge);
        auto adj_facet = compute_vertex_vertex_adjacency(mesh, DualConnectivityType::Facet);

        REQUIRE(adj_edge.get_num_entries() == 3);
        REQUIRE(adj_facet.get_num_entries() == 3);
        for (Index v = 0; v < 3; ++v) {
            auto ne = adj_edge.get_neighbors(v);
            auto nf = adj_facet.get_neighbors(v);
            REQUIRE(ne.size() == 2);
            REQUIRE(nf.size() == 2);
            std::set<Index> se(ne.begin(), ne.end()), sf(nf.begin(), nf.end());
            CHECK(se == sf);
        }
    }

    SECTION("single quad - facet adds diagonals")
    {
        // Quad {0,1,3,2}: edges are (0,1),(1,3),(3,2),(2,0).
        // Facet connectivity also includes diagonals (0,3) and (1,2).
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0}); // v0
        mesh.add_vertex({1, 0, 0}); // v1
        mesh.add_vertex({0, 1, 0}); // v2
        mesh.add_vertex({1, 1, 0}); // v3
        mesh.add_quad(0, 1, 3, 2);

        auto adj_edge = compute_vertex_vertex_adjacency(mesh, DualConnectivityType::Edge);
        auto adj_facet = compute_vertex_vertex_adjacency(mesh, DualConnectivityType::Facet);

        // Edge: each corner vertex has 2 edge neighbors.
        for (Index v = 0; v < 4; ++v) {
            REQUIRE(adj_edge.get_neighbors(v).size() == 2);
        }
        // Facet: each vertex is adjacent to all 3 others (including diagonal).
        for (Index v = 0; v < 4; ++v) {
            REQUIRE(adj_facet.get_neighbors(v).size() == 3);
        }
    }

    SECTION("two disconnected triangles")
    {
        // f0: {0,1,2}, f1: {3,4,5} — no shared vertices.
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({2, 0, 0});
        mesh.add_vertex({3, 0, 0});
        mesh.add_vertex({2, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(3, 4, 5);

        auto adj = compute_vertex_vertex_adjacency(mesh, DualConnectivityType::Facet);
        REQUIRE(adj.get_num_entries() == 6);
        // Each vertex adjacent to the 2 others in its own triangle.
        for (Index v = 0; v < 6; ++v) {
            REQUIRE(adj.get_neighbors(v).size() == 2);
        }
    }

    SECTION("degenerate facet with duplicated vertices")
    {
        // Triangle [0, 1, 0]: vertex 0 appears twice.
        // v0 is allocated 2*(n-1)=4 slots but after dedup has fewer unique neighbors.
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_polygon({Index(0), Index(1), Index(0)});

        auto adj = compute_vertex_vertex_adjacency(mesh, DualConnectivityType::Facet);

        const auto v0_nbrs = adj.get_neighbors(Index(0));
        const auto v1_nbrs = adj.get_neighbors(Index(1));
        // v0 and v1 must be mutual neighbors despite the duplicated vertex.
        REQUIRE(std::find(v0_nbrs.begin(), v0_nbrs.end(), Index(1)) != v0_nbrs.end());
        REQUIRE(std::find(v1_nbrs.begin(), v1_nbrs.end(), Index(0)) != v1_nbrs.end());
    }

    SECTION("adjacent facets including a degenerate facet with duplicated vertex")
    {
        // f0=[0,1,2] and f1=[1,3,2] share edge (1,2).
        // f2=[2,1,2] is degenerate: vertex 2 appears twice, so each appearance
        // is allocated n-1=2 slots but after dedup has fewer unique neighbors.
        // Expected non-self neighbors:
        //   v0: {1,2}      (only in f0)
        //   v1: {0,2,3}    (f0 + f1 + f2)
        //   v2: {0,1,3}    (f0 + f1 + f2, self-loop from f2 removed)
        //   v3: {1,2}      (only in f1)
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({-1, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({3, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 3, 2);
        mesh.add_polygon({Index(2), Index(1), Index(2)});

        auto non_self_neighbors = [](const AdjacencyList<Index>& adj, Index v) {
            auto nbrs = adj.get_neighbors(v);
            std::set<Index> s(nbrs.begin(), nbrs.end());
            s.erase(v);
            return s;
        };

        for (auto ct : {DualConnectivityType::Edge, DualConnectivityType::Facet}) {
            auto adj = compute_vertex_vertex_adjacency(mesh, ct);
            CHECK(non_self_neighbors(adj, 0) == std::set<Index>({1, 2}));
            CHECK(non_self_neighbors(adj, 1) == std::set<Index>({0, 2, 3}));
            CHECK(non_self_neighbors(adj, 2) == std::set<Index>({0, 1, 3}));
            CHECK(non_self_neighbors(adj, 3) == std::set<Index>({1, 2}));
        }
    }

    SECTION("hybrid polygon mesh with adjacency and duplicated vertex")
    {
        // 5 vertices: quad q=[0,1,4,3], triangle t=[1,2,4], degenerate d=[4,1,4].
        // q and t share edge (1,4); d is degenerate (v4 appears twice) on the same edge.
        //
        //  3---4
        //  | * |*
        //  | q | t
        //  |*  |*
        //  0---1---2
        //
        // Edge connectivity — only boundary edges, no quad diagonals:
        //   v0: {1,3}     v1: {0,2,4}   v2: {1,4}
        //   v3: {0,4}     v4: {1,2,3}
        //
        // Facet connectivity — quad adds diagonals (0,4) and (1,3):
        //   v0: {1,3,4}   v1: {0,2,3,4}  v2: {1,4}
        //   v3: {0,1,4}   v4: {0,1,2,3}
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0}); // v0
        mesh.add_vertex({1, 0, 0}); // v1
        mesh.add_vertex({2, 0, 0}); // v2
        mesh.add_vertex({0, 1, 0}); // v3
        mesh.add_vertex({1, 1, 0}); // v4
        mesh.add_quad(0, 1, 4, 3);
        mesh.add_triangle(1, 2, 4);
        mesh.add_polygon({Index(4), Index(1), Index(4)}); // degenerate: v4 appears twice

        auto non_self_neighbors = [](const AdjacencyList<Index>& adj, Index v) {
            auto nbrs = adj.get_neighbors(v);
            std::set<Index> s(nbrs.begin(), nbrs.end());
            s.erase(v);
            return s;
        };

        {
            auto adj = compute_vertex_vertex_adjacency(mesh, DualConnectivityType::Edge);
            CHECK(non_self_neighbors(adj, 0) == std::set<Index>({1, 3}));
            CHECK(non_self_neighbors(adj, 1) == std::set<Index>({0, 2, 4}));
            CHECK(non_self_neighbors(adj, 2) == std::set<Index>({1, 4}));
            CHECK(non_self_neighbors(adj, 3) == std::set<Index>({0, 4}));
            CHECK(non_self_neighbors(adj, 4) == std::set<Index>({1, 2, 3}));
        }
        {
            auto adj = compute_vertex_vertex_adjacency(mesh, DualConnectivityType::Facet);
            CHECK(non_self_neighbors(adj, 0) == std::set<Index>({1, 3, 4}));
            CHECK(non_self_neighbors(adj, 1) == std::set<Index>({0, 2, 3, 4}));
            CHECK(non_self_neighbors(adj, 2) == std::set<Index>({1, 4}));
            CHECK(non_self_neighbors(adj, 3) == std::set<Index>({0, 1, 4}));
            CHECK(non_self_neighbors(adj, 4) == std::set<Index>({0, 1, 2, 3}));
        }
    }

    SECTION("facet connectivity is superset of edge connectivity")
    {
        // For any mesh, every edge neighbor is also a facet neighbor.
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_vertex({2, 0, 0});
        mesh.add_quad(0, 1, 3, 2);
        mesh.add_triangle(1, 4, 3);

        auto adj_edge = compute_vertex_vertex_adjacency(mesh, DualConnectivityType::Edge);
        auto adj_facet = compute_vertex_vertex_adjacency(mesh, DualConnectivityType::Facet);

        for (Index v = 0; v < mesh.get_num_vertices(); ++v) {
            auto ne = adj_edge.get_neighbors(v);
            auto nf = adj_facet.get_neighbors(v);
            std::set<Index> sf(nf.begin(), nf.end());
            for (Index u : ne) {
                CHECK(sf.count(u) == 1);
            }
        }
    }
}

TEST_CASE(
    "compute_vertex_vertex_adjacency benchmark",
    "[surface][adjacency][utilities][!benchmark]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");
    BENCHMARK("compute_vertex_vertex_adjacency")
    {
        return compute_vertex_vertex_adjacency(mesh);
    };

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    using MeshType = TriangleMesh3D;
    BENCHMARK_ADVANCED("Mesh::initialize_connectivity")(Catch::Benchmark::Chronometer meter)
    {
        auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);
        meter.measure([&]() {
            legacy_mesh->initialize_connectivity();
            return legacy_mesh->get_vertex_vertex_adjacency();
        });
    };
#endif
}

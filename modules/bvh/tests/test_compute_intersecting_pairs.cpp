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

#include <lagrange/bvh/compute_intersecting_pairs.h>

#include <set>

TEST_CASE("bvh::compute_intersecting_pairs", "[bvh][intersecting_pairs]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SECTION("Empty mesh")
    {
        SurfaceMesh<Scalar, Index> mesh(3);
        auto intersections = bvh::compute_intersecting_pairs(mesh);
        REQUIRE(intersections.get_num_entries() == 0);
    }

    SECTION("Single triangle")
    {
        SurfaceMesh<Scalar, Index> mesh(3);
        mesh.add_vertex({0.0, 0.0, 0.0});
        mesh.add_vertex({1.0, 0.0, 0.0});
        mesh.add_vertex({0.0, 1.0, 0.0});
        mesh.add_triangle(0, 1, 2);

        auto intersections = bvh::compute_intersecting_pairs(mesh);
        REQUIRE(intersections.get_num_entries() == 1);
        REQUIRE(intersections.get_neighbors(0).size() == 0);
    }

    SECTION("Two non-intersecting triangles")
    {
        SurfaceMesh<Scalar, Index> mesh(3);
        // Triangle 1 in XY plane at z=0
        mesh.add_vertex({0.0, 0.0, 0.0});
        mesh.add_vertex({1.0, 0.0, 0.0});
        mesh.add_vertex({0.0, 1.0, 0.0});
        mesh.add_triangle(0, 1, 2);

        // Triangle 2 in XY plane at z=1 (parallel, separated)
        mesh.add_vertex({0.0, 0.0, 1.0});
        mesh.add_vertex({1.0, 0.0, 1.0});
        mesh.add_vertex({0.0, 1.0, 1.0});
        mesh.add_triangle(3, 4, 5);

        auto intersections = bvh::compute_intersecting_pairs(mesh);
        REQUIRE(intersections.get_num_entries() == 2);
        REQUIRE(intersections.get_neighbors(0).size() == 0);
        REQUIRE(intersections.get_neighbors(1).size() == 0);
    }

    SECTION("Two adjacent triangles (sharing edge)")
    {
        SurfaceMesh<Scalar, Index> mesh(3);
        mesh.add_vertex({0.0, 0.0, 0.0});
        mesh.add_vertex({1.0, 0.0, 0.0});
        mesh.add_vertex({0.5, 1.0, 0.0});
        mesh.add_vertex({0.5, -1.0, 0.0});

        // Two triangles sharing edge (0, 1)
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 1, 3);

        // Edge-adjacent triangles without interior overlap must not be reported.
        // The geometric test (include_boundary=false) correctly returns false for them.
        auto intersections = bvh::compute_intersecting_pairs(mesh);
        REQUIRE(intersections.get_num_entries() == 2);
        REQUIRE(intersections.get_neighbors(0).size() == 0);
        REQUIRE(intersections.get_neighbors(1).size() == 0);
    }

    SECTION("Two intersecting triangles")
    {
        SurfaceMesh<Scalar, Index> mesh(3);
        // Triangle 1 in XY plane
        mesh.add_vertex({-1.0, 0.0, 0.0});
        mesh.add_vertex({1.0, 0.0, 0.0});
        mesh.add_vertex({0.0, 1.0, 0.0});
        mesh.add_triangle(0, 1, 2);

        // Triangle 2 crossing through triangle 1
        mesh.add_vertex({0.0, 0.5, -1.0});
        mesh.add_vertex({0.0, 0.5, 1.0});
        mesh.add_vertex({0.0, -0.5, 0.0});
        mesh.add_triangle(3, 4, 5);

        auto intersections = bvh::compute_intersecting_pairs(mesh);
        REQUIRE(intersections.get_num_entries() == 2);
        auto n0 = intersections.get_neighbors(0);
        auto n1 = intersections.get_neighbors(1);
        REQUIRE(n0.size() == 1);
        REQUIRE(n1.size() == 1);
        REQUIRE(n0[0] == 1);
        REQUIRE(n1[0] == 0);
    }

    SECTION("Cube with self-intersecting face")
    {
        SurfaceMesh<Scalar, Index> mesh(3);
        // Simple quad split into triangles
        mesh.add_vertex({0.0, 0.0, 0.0});
        mesh.add_vertex({1.0, 0.0, 0.0});
        mesh.add_vertex({1.0, 1.0, 0.0});
        mesh.add_vertex({0.0, 1.0, 0.0});

        // Two triangles forming a quad
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 2, 3);

        // Add an intersecting triangle in the middle
        mesh.add_vertex({0.5, 0.5, -0.5});
        mesh.add_vertex({0.5, 0.5, 0.5});
        mesh.add_vertex({0.25, 0.25, 0.0});
        mesh.add_triangle(4, 5, 6);

        auto intersections = bvh::compute_intersecting_pairs(mesh);
        REQUIRE(intersections.get_num_entries() == 3);
        // The crossing triangle (index 2) must intersect both triangles of the quad (0 and 1)
        auto n2 = intersections.get_neighbors(2);
        REQUIRE(n2.size() == 2);
        std::set<Index> n2_actual(n2.begin(), n2.end());
        std::set<Index> n2_expected{Index(0), Index(1)};
        REQUIRE(n2_actual == n2_expected);
    }

    SECTION("Topologically duplicate triangles")
    {
        // Two triangles referencing the exact same three vertices are spatially identical:
        // their interiors overlap completely and must be reported as intersecting.
        SurfaceMesh<Scalar, Index> mesh(3);
        mesh.add_vertex({0.0, 0.0, 0.0});
        mesh.add_vertex({1.0, 0.0, 0.0});
        mesh.add_vertex({0.0, 1.0, 0.0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 1, 2); // exact duplicate

        auto intersections = bvh::compute_intersecting_pairs(mesh);
        REQUIRE(intersections.get_num_entries() == 2);
        REQUIRE(intersections.get_neighbors(0).size() == 1);
        REQUIRE(intersections.get_neighbors(1).size() == 1);
        REQUIRE(intersections.get_neighbors(0)[0] == Index(1));
        REQUIRE(intersections.get_neighbors(1)[0] == Index(0));
    }

    SECTION("Vertex-adjacent triangles with interior overlap")
    {
        // T0 lies in the z=0 plane. T1 shares vertex (0,0,0) with T0 but its edge
        // (2,1,-2)->(2,1,2) pierces T0's interior at (2,1,0), which is strictly inside T0.
        // The shared vertex is boundary contact and must not prevent detection of the
        // interior crossing.
        SurfaceMesh<Scalar, Index> mesh(3);
        mesh.add_vertex({0.0, 0.0, 0.0}); // 0 (shared)
        mesh.add_vertex({4.0, 0.0, 0.0}); // 1
        mesh.add_vertex({2.0, 4.0, 0.0}); // 2
        mesh.add_triangle(0, 1, 2); // T0 in z=0 plane

        mesh.add_vertex({2.0, 1.0, -2.0}); // 3
        mesh.add_vertex({2.0, 1.0, 2.0}); // 4
        mesh.add_triangle(0, 3, 4); // T1: shares vertex 0; edge 3-4 pierces T0's interior

        auto intersections = bvh::compute_intersecting_pairs(mesh);
        REQUIRE(intersections.get_num_entries() == 2);
        REQUIRE(intersections.get_neighbors(0).size() == 1);
        REQUIRE(intersections.get_neighbors(1).size() == 1);
        REQUIRE(intersections.get_neighbors(0)[0] == Index(1));
        REQUIRE(intersections.get_neighbors(1)[0] == Index(0));
    }

    SECTION("Non-coplanar triangles sharing a vertex with crossing opposite edges")
    {
        // T0 lies in the y=0 plane; T1 lies in the x=z plane. They share vertex V0=(0,0,0).
        // The opposite edge of T0 (V1=(2,0,0) -- V2=(0,0,2)) and the opposite edge of T1
        // (V3=(1,-1,1) -- V4=(1,1,1)) both pass through (1,0,1), so the opposite edges
        // intersect. The intersection line of the two planes (x=z, y=0) runs from V0 to
        // (1,0,1), and its interior is strictly inside both triangles, so the pair must be
        // reported.
        SurfaceMesh<Scalar, Index> mesh(3);
        mesh.add_vertex({0.0, 0.0, 0.0}); // 0 (shared)
        mesh.add_vertex({2.0, 0.0, 0.0}); // 1
        mesh.add_vertex({0.0, 0.0, 2.0}); // 2
        mesh.add_triangle(0, 1, 2); // T0 in y=0 plane

        mesh.add_vertex({1.0, -1.0, 1.0}); // 3
        mesh.add_vertex({1.0, 1.0, 1.0}); // 4
        mesh.add_triangle(0, 3, 4); // T1 in x=z plane; shares vertex 0

        auto intersections = bvh::compute_intersecting_pairs(mesh);
        REQUIRE(intersections.get_num_entries() == 2);
        REQUIRE(intersections.get_neighbors(0).size() == 1);
        REQUIRE(intersections.get_neighbors(1).size() == 1);
        REQUIRE(intersections.get_neighbors(0)[0] == Index(1));
        REQUIRE(intersections.get_neighbors(1)[0] == Index(0));
    }

    SECTION("Non-triangle mesh should throw")
    {
        SurfaceMesh<Scalar, Index> mesh(3);
        mesh.add_vertex({0.0, 0.0, 0.0});
        mesh.add_vertex({1.0, 0.0, 0.0});
        mesh.add_vertex({1.0, 1.0, 0.0});
        mesh.add_vertex({0.0, 1.0, 0.0});
        mesh.add_quad(0, 1, 2, 3);

        REQUIRE_THROWS(bvh::compute_intersecting_pairs(mesh));
    }

    SECTION("Multiple intersections")
    {
        SurfaceMesh<Scalar, Index> mesh(3);

        // Create a mesh with known intersections
        // Triangle 0: horizontal at z=0
        mesh.add_vertex({-2.0, -2.0, 0.0});
        mesh.add_vertex({2.0, -2.0, 0.0});
        mesh.add_vertex({0.0, 2.0, 0.0});
        mesh.add_triangle(0, 1, 2);

        // Triangle 1: vertical crossing triangle 0
        mesh.add_vertex({0.0, 0.0, -1.0});
        mesh.add_vertex({0.0, 0.0, 1.0});
        mesh.add_vertex({1.0, 0.0, 0.0});
        mesh.add_triangle(3, 4, 5);

        // Triangle 2: another vertical crossing triangle 0
        mesh.add_vertex({-0.5, 0.0, -1.0});
        mesh.add_vertex({-0.5, 0.0, 1.0});
        mesh.add_vertex({-1.5, 0.0, 0.0});
        mesh.add_triangle(6, 7, 8);

        auto intersections = bvh::compute_intersecting_pairs(mesh);
        REQUIRE(intersections.get_num_entries() == 3);

        // Triangle 0 should intersect with both triangle 1 and 2
        auto n0 = intersections.get_neighbors(0);
        REQUIRE(n0.size() == 2);
        std::set<Index> s0(n0.begin(), n0.end());
        REQUIRE(s0.count(1) == 1);
        REQUIRE(s0.count(2) == 1);

        // Triangle 1 should intersect with triangle 0
        auto n1 = intersections.get_neighbors(1);
        REQUIRE(n1.size() == 1);
        REQUIRE(n1[0] == 0);

        // Triangle 2 should intersect with triangle 0
        auto n2 = intersections.get_neighbors(2);
        REQUIRE(n2.size() == 1);
        REQUIRE(n2[0] == 0);
    }

    SECTION("Real mesh - ball.obj")
    {
        auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/ball.obj");

        REQUIRE(mesh.is_triangle_mesh());
        REQUIRE(mesh.get_num_facets() > 0);

        auto intersections = bvh::compute_intersecting_pairs(mesh);

        // Count total intersecting pairs and check specific pair
        Index total_intersections = 0;
        bool found_0_4801 = false;

        for (Index i = 0; i < intersections.get_num_entries(); ++i) {
            auto neighbors = intersections.get_neighbors(i);
            for (Index j : neighbors) {
                if (i < j) {
                    total_intersections++;
                }
                if ((i == 0 && j == 4801) || (i == 4801 && j == 0)) {
                    found_0_4801 = true;
                }
            }
        }

        REQUIRE_FALSE(found_0_4801);

        // Ball.obj is a valid sphere mesh: no pair of facets has overlapping interiors.
        REQUIRE(total_intersections == 0);
    }
}

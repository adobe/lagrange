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
#include <lagrange/topology.h>

TEST_CASE("Topology", "[surface][topology]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SECTION("single triangle")
    {
        SurfaceMesh<Scalar, Index> mesh;
        Scalar v0[] = {0, 0, 0};
        Scalar v1[] = {1, 0, 0};
        Scalar v2[] = {0, 1, 0};
        mesh.add_vertex(v0);
        mesh.add_vertex(v1);
        mesh.add_vertex(v2);
        mesh.add_triangle(0, 1, 2);

        REQUIRE(compute_euler(mesh) == 1);
        REQUIRE(is_vertex_manifold(mesh));
        REQUIRE(is_edge_manifold(mesh));
    }

    SECTION("bow tie")
    {
        SurfaceMesh<Scalar, Index> mesh;
        Scalar v0[] = {0, 0, 0};
        Scalar v1[] = {1, 0, 0};
        Scalar v2[] = {0, 1, 0};
        Scalar v3[] = {-1, 0, 0};
        Scalar v4[] = {0, -1, 0};
        mesh.add_vertex(v0);
        mesh.add_vertex(v1);
        mesh.add_vertex(v2);
        mesh.add_vertex(v3);
        mesh.add_vertex(v4);
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 4, 3);

        REQUIRE(compute_euler(mesh) == 1);
        REQUIRE(!is_vertex_manifold(mesh));
        REQUIRE(is_edge_manifold(mesh));
    }

    SECTION("non-manifold edge")
    {
        SurfaceMesh<Scalar, Index> mesh;
        Scalar v0[] = {0, 0, 0};
        Scalar v1[] = {1, 0, 0};
        Scalar v2[] = {0, 1, 0};
        Scalar v3[] = {-1, 0, 0};
        Scalar v4[] = {0, -1, 0};
        mesh.add_vertex(v0);
        mesh.add_vertex(v1);
        mesh.add_vertex(v2);
        mesh.add_vertex(v3);
        mesh.add_vertex(v4);
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 1, 3);
        mesh.add_triangle(0, 1, 4);

        REQUIRE(compute_euler(mesh) == 1);
        REQUIRE(!is_vertex_manifold(mesh));
        REQUIRE(!is_edge_manifold(mesh));
    }

    SECTION("two triangles")
    {
        SurfaceMesh<Scalar, Index> mesh;
        Scalar v0[] = {0, 0, 0};
        Scalar v1[] = {1, 0, 0};
        Scalar v2[] = {0, 1, 0};
        Scalar v3[] = {-1, 0, 0};
        Scalar v4[] = {0, -1, 0};
        Scalar v5[] = {-1, -1, 0};
        mesh.add_vertex(v0);
        mesh.add_vertex(v1);
        mesh.add_vertex(v2);
        mesh.add_vertex(v3);
        mesh.add_vertex(v4);
        mesh.add_vertex(v5);
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(3, 4, 5);

        REQUIRE(compute_euler(mesh) == 2);
        REQUIRE(is_vertex_manifold(mesh));
        REQUIRE(is_edge_manifold(mesh));
    }

    SECTION("tet")
    {
        SurfaceMesh<Scalar, Index> mesh;
        Scalar v0[] = {0, 0, 0};
        Scalar v1[] = {1, 0, 0};
        Scalar v2[] = {0, 1, 0};
        Scalar v3[] = {0, 0, 1};
        mesh.add_vertex(v0);
        mesh.add_vertex(v1);
        mesh.add_vertex(v2);
        mesh.add_vertex(v3);
        mesh.add_triangle(0, 2, 1);
        mesh.add_triangle(0, 1, 3);
        mesh.add_triangle(1, 2, 3);
        mesh.add_triangle(2, 0, 3);

        REQUIRE(compute_euler(mesh) == 2);
        REQUIRE(is_vertex_manifold(mesh));
        REQUIRE(is_edge_manifold(mesh));
    }

    SECTION("two tets sharing a vertex")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({0, 0, 1});
        mesh.add_vertex({-1, 0, 0});
        mesh.add_vertex({0, -1, 0});
        mesh.add_vertex({0, 0, -1});
        mesh.add_triangle(0, 2, 1);
        mesh.add_triangle(0, 1, 3);
        mesh.add_triangle(1, 2, 3);
        mesh.add_triangle(2, 0, 3);
        mesh.add_triangle(4, 5, 0);
        mesh.add_triangle(5, 6, 0);
        mesh.add_triangle(6, 4, 0);
        mesh.add_triangle(4, 6, 5);

        REQUIRE(compute_euler(mesh) == 3);
        REQUIRE(!is_vertex_manifold(mesh));
        REQUIRE(is_edge_manifold(mesh));
    }

    SECTION("isolated vertices")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        REQUIRE(compute_euler(mesh) == 1);
        REQUIRE(is_vertex_manifold(mesh));
        REQUIRE(is_edge_manifold(mesh));
    }
}

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

#include <lagrange/compute_area.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/mesh_cleanup/split_long_edges.h>
#include <lagrange/orientation.h>
#include <lagrange/remeshing_im/remesh.h>
#include <lagrange/topology.h>
#include <lagrange/views.h>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <numeric>
#include <vector>

TEST_CASE("remeshing_im::remesh", "[remeshing_im]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({0.5, 0.5, 1});
    mesh.add_triangle(0, 2, 1);
    mesh.add_triangle(0, 3, 2);
    mesh.add_triangle(0, 1, 4);
    mesh.add_triangle(1, 2, 4);
    mesh.add_triangle(2, 3, 4);
    mesh.add_triangle(3, 0, 4);
    const Scalar total_area = compute_mesh_area(mesh);

    remeshing_im::RemeshingOptions options;
    options.crease_angle = 45.f;
    options.deterministic = true;

    SECTION("default options")
    {
        options.target_num_facets = 100;
        auto out_mesh = remeshing_im::remesh(mesh, options);

        REQUIRE(out_mesh.is_quad_mesh());
        REQUIRE(is_vertex_manifold(out_mesh));
        REQUIRE(is_edge_manifold(out_mesh));
        REQUIRE(is_oriented(out_mesh));

        REQUIRE_THAT(compute_mesh_area(out_mesh), Catch::Matchers::WithinRel(total_area, 1e-1));
    }

    SECTION("Triangle mesh")
    {
        options.target_num_facets = 500;
        options.posy = remeshing_im::PosyType::Triangle;
        options.rosy = remeshing_im::RosyType::Hex;
        options.crease_angle = 45.f;
        auto out_mesh = remeshing_im::remesh(mesh, options);
        io::save_mesh("out_mesh.obj", out_mesh);

        REQUIRE(out_mesh.is_triangle_mesh());
        REQUIRE(is_vertex_manifold(out_mesh));
        REQUIRE(is_edge_manifold(out_mesh));
        REQUIRE(is_oriented(out_mesh));

        REQUIRE_THAT(compute_mesh_area(out_mesh), Catch::Matchers::WithinRel(total_area, 1e-1));
    }

    SECTION("empty mesh")
    {
        SurfaceMesh<Scalar, Index> empty_mesh;
        auto out_mesh = remeshing_im::remesh(empty_mesh);
        REQUIRE(out_mesh.is_quad_mesh());
        REQUIRE(out_mesh.get_num_vertices() == 0);
        REQUIRE(out_mesh.get_num_facets() == 0);
        REQUIRE(is_vertex_manifold(out_mesh));
        REQUIRE(is_edge_manifold(out_mesh));
        REQUIRE(is_oriented(out_mesh));
    }
}

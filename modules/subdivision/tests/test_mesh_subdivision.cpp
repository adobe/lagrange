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
#include <lagrange/compute_euler.h>
#include <lagrange/compute_facet_area.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/create_mesh.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/subdivision/mesh_subdivision.h>
#include <lagrange/subdivision/midpoint_subdivision.h>
#include <lagrange/subdivision/sqrt_subdivision.h>
#include <lagrange/utils/safe_cast.h>

#include <lagrange/testing/common.h>
#include <catch2/catch_approx.hpp>



using namespace lagrange;

template <typename InputMesh, typename OutputMesh>
void validate_catmull_clark_subdivision(
    InputMesh& mesh,
    OutputMesh& subdiv_mesh,
    int num_subdivisions)
{
    using Index = typename InputMesh::Index;

    bool triangulated = subdiv_mesh.get_vertex_per_facet() == 3;
    auto original_num_facets = mesh.get_num_facets();
    REQUIRE(
        subdiv_mesh.get_num_facets() == original_num_facets * mesh.get_vertex_per_facet() *
                                            pow(4, num_subdivisions - 1) * (triangulated ? 2 : 1));

    if (num_subdivisions == 1) {
        // Higher subdivisions require connectivity data from previous mesh
        // Checking for lowest level of subdivision..

        // Initialize connectivity of original mesh
        mesh.initialize_connectivity();
        la_runtime_assert(mesh.is_connectivity_initialized());

        mesh.initialize_edge_data();
        la_runtime_assert(mesh.is_edge_data_initialized());

        auto original_num_vertices = mesh.get_num_vertices();
        auto original_num_edges = mesh.get_num_edges();

        REQUIRE(
            subdiv_mesh.get_num_vertices() ==
            safe_cast<Index>(original_num_vertices + original_num_facets + original_num_edges));
    }

    // Check Euler characteristic
    auto euler_src = lagrange::compute_euler(mesh);
    auto euler_dst = lagrange::compute_euler(subdiv_mesh);
    REQUIRE(euler_dst == euler_src);
}

template <typename InputMesh, typename OutputMesh>
void validate_loop_subdivision(InputMesh& mesh, OutputMesh& subdiv_mesh, int num_subdivisions)
{
    // Output mesh is a triangle mesh.
    REQUIRE(subdiv_mesh.get_vertex_per_facet() == 3);

    auto original_num_facets = mesh.get_num_facets();
    // auto original_num_vertices = mesh.get_num_vertices();

    REQUIRE(subdiv_mesh.get_num_facets() == pow(4, num_subdivisions) * original_num_facets);
    // REQUIRE(subdiv_mesh.get_num_vertices()...

    // Check Euler characteristic
    auto euler_src = lagrange::compute_euler(mesh);
    auto euler_dst = lagrange::compute_euler(subdiv_mesh);
    REQUIRE(euler_dst == euler_src);
}

TEST_CASE("mesh_subdivision_catmull_clark_zero_level", "[mesh][subdivision][catmull_clark]")
{
    subdivision::SubdivisionScheme scheme = subdivision::SubdivisionScheme::SCHEME_CATMARK;

    SECTION("Triangle Mesh to Triangle Mesh")
    {
        auto mesh = lagrange::testing::load_mesh<TriangleMesh3D>("open/core/simple/cube.obj");
        auto subdivided_mesh =
            subdivision::subdivide_mesh<TriangleMesh3D, TriangleMesh3D>(*mesh, scheme, 0);

        // Should pass
        lagrange::compute_vertex_normal(*subdivided_mesh);

        // Shouldn't change
        REQUIRE(subdivided_mesh->get_num_facets() == mesh->get_num_facets());
        REQUIRE(subdivided_mesh->get_num_vertices() == mesh->get_num_vertices());
        // Check Euler characteristic
        auto euler_src = lagrange::compute_euler(*mesh);
        auto euler_dst = lagrange::compute_euler(*subdivided_mesh);
        REQUIRE(euler_dst == euler_src);
    }

    SECTION("Quad Mesh to Triangle Mesh")
    {
        auto mesh =
            lagrange::testing::load_mesh<QuadMesh3D>("open/core/simple/quad_meshes/cube.obj");
        auto subdivided_mesh =
            subdivision::subdivide_mesh<QuadMesh3D, TriangleMesh3D>(*mesh, scheme, 0);

        // Should pass
        lagrange::compute_vertex_normal(*subdivided_mesh);

        REQUIRE(subdivided_mesh->get_num_vertices() == mesh->get_num_vertices());
        REQUIRE(subdivided_mesh->get_num_facets() == mesh->get_num_facets() * 2);
        // Check Euler characteristic
        auto euler_src = lagrange::compute_euler(*mesh);
        auto euler_dst = lagrange::compute_euler(*subdivided_mesh);
        REQUIRE(euler_dst == euler_src);
    }

    SECTION("Quad Mesh to Quad Mesh")
    {
        auto mesh =
            lagrange::testing::load_mesh<QuadMesh3D>("open/core/simple/quad_meshes/cube.obj");
        auto subdivided_mesh =
            subdivision::subdivide_mesh<QuadMesh3D, QuadMesh3D>(*mesh, scheme, 0);

        // Shouldn't change
        REQUIRE(subdivided_mesh->get_num_facets() == mesh->get_num_facets());
        REQUIRE(subdivided_mesh->get_num_vertices() == mesh->get_num_vertices());
        // Check Euler characteristic
        auto euler_src = lagrange::compute_euler(*mesh);
        auto euler_dst = lagrange::compute_euler(*subdivided_mesh);
        REQUIRE(euler_dst == euler_src);
    }
}

TEST_CASE("mesh_subdivision_catmull_clark", "[mesh][subdivision][catmull_clark]")
{
    int num_subdivisions = 1;
    subdivision::SubdivisionScheme scheme = subdivision::SubdivisionScheme::SCHEME_CATMARK;

    SECTION("Triangle Mesh to Triangle Mesh")
    {
        auto mesh = lagrange::testing::load_mesh<TriangleMesh3D>("open/core/simple/cube.obj");
        auto subdivided_mesh = subdivision::subdivide_mesh<TriangleMesh3D, TriangleMesh3D>(
            *mesh,
            scheme,
            num_subdivisions);
        // TODO: this is hardcoded to validate the output as a quadmesh
        // validate_catmull_clark_subdivision(*mesh, *subdivided_mesh, num_subdivisions);
    }

    SECTION("Triangle Mesh to Quad Mesh")
    {
        auto mesh = lagrange::testing::load_mesh<TriangleMesh3D>("open/core/simple/cube.obj");
        auto subdivided_mesh = subdivision::subdivide_mesh<TriangleMesh3D, QuadMesh3D>(
            *mesh,
            scheme,
            num_subdivisions);
        validate_catmull_clark_subdivision(*mesh, *subdivided_mesh, num_subdivisions);
    }

    SECTION("Quad Mesh to Quad Mesh")
    {
        auto mesh = lagrange::testing::load_mesh<QuadMesh3D>("open/core/simple/quad_meshes/cube.obj");
        auto subdivided_mesh =
            subdivision::subdivide_mesh<QuadMesh3D, QuadMesh3D>(*mesh, scheme, num_subdivisions);
        validate_catmull_clark_subdivision(*mesh, *subdivided_mesh, num_subdivisions);
    }
}

TEST_CASE(
    "mesh_subdivision_catmull_clark_subdivisions",
    "[mesh][subdivision][catmull_clark][subdivisions]" LA_SLOW_DEBUG_FLAG)
{
    int num_subdivisions = 0;
    subdivision::SubdivisionScheme scheme = subdivision::SubdivisionScheme::SCHEME_CATMARK;

    SECTION("Min subdivisions") { num_subdivisions = 1; }

    SECTION("Max subdivisions") { num_subdivisions = 5; }

    // Triangle Mesh
    auto trimesh = lagrange::testing::load_mesh<TriangleMesh3D>("open/core/simple/cube.obj");
    auto subdivided_trimesh =
        subdivision::subdivide_mesh<TriangleMesh3D, QuadMesh3D>(*trimesh, scheme, num_subdivisions);
    validate_catmull_clark_subdivision(*trimesh, *subdivided_trimesh, num_subdivisions);

    // Quad Mesh
    auto quadmesh = lagrange::testing::load_mesh<QuadMesh3D>("open/core/simple/quad_meshes/cube.obj");
    auto subdivided_quadmesh =
        subdivision::subdivide_mesh<QuadMesh3D, QuadMesh3D>(*quadmesh, scheme, num_subdivisions);
    validate_catmull_clark_subdivision(*quadmesh, *subdivided_quadmesh, num_subdivisions);
}


TEST_CASE("mesh_subdivision_loop", "[mesh][subdivision][loop]" LA_SLOW_DEBUG_FLAG)
{
    int num_subdivisions = 1;
    subdivision::SubdivisionScheme scheme = subdivision::SubdivisionScheme::SCHEME_LOOP;

    SECTION("Triangle Mesh")
    {
        auto mesh = lagrange::testing::load_mesh<TriangleMesh3D>("open/core/simple/octahedron.obj");
        auto subdivided_mesh = subdivision::subdivide_mesh<TriangleMesh3D, TriangleMesh3D>(
            *mesh,
            scheme,
            num_subdivisions);
        validate_loop_subdivision(*mesh, *subdivided_mesh, num_subdivisions);
    }

    SECTION("Invalid Mesh")
    {
        auto mesh = lagrange::testing::load_mesh<QuadMesh3D>("open/core/simple/quad_meshes/cube.obj");
        LA_REQUIRE_THROWS(subdivision::subdivide_mesh<QuadMesh3D, TriangleMesh3D>(
            *mesh,
            scheme,
            num_subdivisions));
    }
}

TEST_CASE("mesh_subdivision_loop_subdivisions", "[mesh][subdivision][loop][subdivisions]" LA_SLOW_DEBUG_FLAG)
{
    int num_subdivisions = 0;
    subdivision::SubdivisionScheme scheme = subdivision::SubdivisionScheme::SCHEME_LOOP;

    SECTION("Min subdivisions") { num_subdivisions = 1; }

    SECTION("Max subdivisions") { num_subdivisions = 5; }

    auto mesh = lagrange::testing::load_mesh<TriangleMesh3D>("open/core/simple/cube.obj");
    auto subdivided_mesh = subdivision::subdivide_mesh<TriangleMesh3D, TriangleMesh3D>(
        *mesh,
        scheme,
        num_subdivisions);
    validate_loop_subdivision(*mesh, *subdivided_mesh, num_subdivisions);
}

TEST_CASE("mesh_subdivision_with_uv", "[mesh][subdivision]")
{
    using Scalar = float;
    using Index = int;
    using VertexArray = Eigen::Matrix<Scalar, Eigen::Dynamic, 3>;
    using FacetArray = Eigen::Matrix<Index, Eigen::Dynamic, 3>;
    using MeshType = lagrange::Mesh<VertexArray, FacetArray>;

    constexpr subdivision::SubdivisionScheme scheme = subdivision::SubdivisionScheme::SCHEME_LOOP;
    constexpr int levels = 2;

    VertexArray vertices(4, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 0.0;
    FacetArray facets(2, 3);
    facets << 0, 1, 2, 0, 2, 3;
    auto mesh = wrap_with_mesh(vertices, facets);
    mesh->initialize_uv(vertices.template leftCols<2>().eval(), facets);
    REQUIRE(mesh->is_uv_initialized());

    auto subdivided_mesh = subdivision::subdivide_mesh<MeshType, MeshType>(*mesh, scheme, levels);
    REQUIRE(subdivided_mesh->is_uv_initialized());
    validate_loop_subdivision(*mesh, *subdivided_mesh, levels);

    auto uv_mesh = subdivided_mesh->get_uv_mesh();
    compute_facet_area(*mesh);
    compute_facet_area(*uv_mesh);
    const auto& areas = mesh->get_facet_attribute("area");
    const auto& uv_areas = uv_mesh->get_facet_attribute("area");
    REQUIRE(areas.sum() == Catch::Approx(uv_areas.sum()));
}

TEST_CASE("mesh_subdivision_sqrt", "[mesh][subdivision][sqrt]")
{
    auto mesh = lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/subdivision/sphere.ply");
    auto expected_mesh = lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/subdivision/sphere_sqrt.ply");
    auto subdivided_mesh = lagrange::subdivision::sqrt_subdivision(*mesh);
    REQUIRE(subdivided_mesh->get_vertices() == expected_mesh->get_vertices());
    REQUIRE(subdivided_mesh->get_facets() == expected_mesh->get_facets());
}

TEST_CASE("mesh_subdivision_midpoint", "[mesh][subdivision][sqrt]")
{
    auto mesh = lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/subdivision/sphere.ply");
    auto expected_mesh = lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/subdivision/sphere_midpoint.ply");
    auto subdivided_mesh = lagrange::subdivision::midpoint_subdivision(*mesh);
    REQUIRE(subdivided_mesh->get_vertices() == expected_mesh->get_vertices());
    REQUIRE(subdivided_mesh->get_facets() == expected_mesh->get_facets());
}

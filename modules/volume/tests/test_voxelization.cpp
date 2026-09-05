/*
 * Copyright 2021 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/create_mesh.h>
#include <lagrange/internal/constants.h>
#include <lagrange/mesh_convert.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>
#include <lagrange/volume/mesh_to_volume.h>
#include <lagrange/volume/volume_to_mesh.h>

#include <cmath>

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
TEST_CASE("voxelization: reproducibility (legacy)", "[volume]")
{
    auto mesh = lagrange::create_sphere();
    auto grid = lagrange::volume::mesh_to_volume(*mesh, 0.1);
    auto grid2 = lagrange::volume::mesh_to_volume(*mesh, 0.1);
    auto mesh2 = lagrange::volume::volume_to_mesh_legacy<lagrange::TriangleMesh3Df>(*grid);
    auto mesh3 = lagrange::volume::volume_to_mesh_legacy<lagrange::TriangleMesh3Df>(*grid2);
    REQUIRE(mesh2->get_num_vertices() > 0);
    REQUIRE(mesh2->get_num_facets() > 0);
    REQUIRE(mesh2->get_vertices() == mesh3->get_vertices());
    REQUIRE(mesh2->get_facets() == mesh3->get_facets());
}
#endif

TEST_CASE("voxelization: reproducibility", "[volume]")
{
    using Scalar = float;
    using Index = uint32_t;
    using SurfaceMeshType = lagrange::SurfaceMesh<Scalar, Index>;
    auto mesh = lagrange::to_surface_mesh_copy<Scalar, Index>(*lagrange::create_sphere());
    lagrange::volume::MeshToVolumeOptions m2v_opt;
    m2v_opt.voxel_size = 0.1;
    auto grid = lagrange::volume::mesh_to_volume(mesh, m2v_opt);
    auto grid2 = lagrange::volume::mesh_to_volume(mesh, m2v_opt);
    auto mesh2 = lagrange::volume::volume_to_mesh<SurfaceMeshType>(*grid);
    auto mesh3 = lagrange::volume::volume_to_mesh<SurfaceMeshType>(*grid2);
    REQUIRE(mesh2.get_num_vertices() > 0);
    REQUIRE(mesh2.get_num_facets() > 0);
    REQUIRE(vertex_view(mesh2) == vertex_view(mesh3));
    REQUIRE(facet_view(mesh2) == facet_view(mesh3));
}

TEST_CASE("voxelization: winding number", "[volume]")
{
    using Scalar = float;
    using Index = uint32_t;
    using SurfaceMeshType = lagrange::SurfaceMesh<Scalar, Index>;
    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/stanford-bunny.obj");
    lagrange::volume::MeshToVolumeOptions m2v_opt;
    m2v_opt.signing_method = lagrange::volume::MeshToVolumeOptions::Sign::FloodFill;
    auto grid = lagrange::volume::mesh_to_volume(mesh, m2v_opt);
    m2v_opt.signing_method = lagrange::volume::MeshToVolumeOptions::Sign::WindingNumber;
    auto grid2 = lagrange::volume::mesh_to_volume(mesh, m2v_opt);
    auto mesh2 = lagrange::volume::volume_to_mesh<SurfaceMeshType>(*grid);
    auto mesh3 = lagrange::volume::volume_to_mesh<SurfaceMeshType>(*grid2);
    // Winding number result should have more vertices/facets than the flood-fill result
    REQUIRE(mesh3.get_num_vertices() > mesh2.get_num_vertices());
    REQUIRE(mesh3.get_num_facets() > mesh2.get_num_facets());
}

TEST_CASE("mesh_to_volume: configurable narrow band", "[volume]")
{
    auto mesh = lagrange::to_surface_mesh_copy<float, uint32_t>(*lagrange::create_cube());

    lagrange::volume::MeshToVolumeOptions options;
    options.voxel_size = 0.1;
    const auto narrow = lagrange::volume::mesh_to_volume(mesh, options);

    SECTION("wide exterior")
    {
        options.exterior_bandwidth = 8.0f;
        const auto wide = lagrange::volume::mesh_to_volume(mesh, options);

        const auto narrow_bbox = narrow->evalActiveVoxelBoundingBox();
        const auto wide_bbox = wide->evalActiveVoxelBoundingBox();
        REQUIRE(wide->activeVoxelCount() > narrow->activeVoxelCount());
        REQUIRE(
            wide_bbox.max().z() - wide_bbox.min().z() >
            narrow_bbox.max().z() - narrow_bbox.min().z());
    }

    SECTION("wide interior")
    {
        options.interior_bandwidth = 8.0f;
        const auto wide = lagrange::volume::mesh_to_volume(mesh, options);

        REQUIRE(wide->activeVoxelCount() > narrow->activeVoxelCount());
        const openvdb::Vec3d world_position(0.5, 0, 0);
        const auto coord = openvdb::Coord::round(wide->worldToIndex(world_position));
        REQUIRE_FALSE(narrow->getConstAccessor().isValueOn(coord));
        REQUIRE(wide->getConstAccessor().isValueOn(coord));
        REQUIRE(wide->getConstAccessor().getValue(coord) < 0);
    }
}

TEST_CASE("mesh_to_volume: edge facets", "[volume]")
{
    using Scalar = float;
    using Index = uint32_t;

    lagrange::volume::MeshToVolumeOptions m2v_opt;
    m2v_opt.signing_method = lagrange::volume::MeshToVolumeOptions::Sign::Unsigned;
    m2v_opt.voxel_size = 0.1;

    // For unsigned grids, extract isosurface at voxel_size * sqrt(3).
    lagrange::volume::VolumeToMeshOptions v2m_opt;
    v2m_opt.isovalue = m2v_opt.voxel_size * std::sqrt(3.0);

    SECTION("pure edge mesh")
    {
        // Inline wireframe unit cube [0,1]^3
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(8);
        auto vertices = vertex_ref(mesh);
        vertices.row(0) << 0, 0, 0;
        vertices.row(1) << 1, 0, 0;
        vertices.row(2) << 1, 1, 0;
        vertices.row(3) << 0, 1, 0;
        vertices.row(4) << 0, 0, 1;
        vertices.row(5) << 1, 0, 1;
        vertices.row(6) << 1, 1, 1;
        vertices.row(7) << 0, 1, 1;
        // 12 edges of the cube
        for (auto [a, b] :
             {std::pair{0u, 1u},
              {1u, 2u},
              {2u, 3u},
              {3u, 0u},
              {4u, 5u},
              {5u, 6u},
              {6u, 7u},
              {7u, 4u},
              {0u, 4u},
              {1u, 5u},
              {2u, 6u},
              {3u, 7u}}) {
            mesh.add_polygon({a, b});
        }
        REQUIRE(mesh.is_regular());
        REQUIRE(mesh.get_vertex_per_facet() == 2);
        auto grid = lagrange::volume::mesh_to_volume(mesh, m2v_opt);
        REQUIRE(grid->activeVoxelCount() > 0);
        auto out =
            lagrange::volume::volume_to_mesh<lagrange::SurfaceMesh<Scalar, Index>>(*grid, v2m_opt);
        REQUIRE(out.get_num_facets() > 0);
    }

    SECTION("hybrid mesh with edges and triangles")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(4);
        auto vertices = vertex_ref(mesh);
        vertices.row(0) << 0, 0, 0;
        vertices.row(1) << 1, 0, 0;
        vertices.row(2) << 0.5f, 1, 0;
        vertices.row(3) << 0.5f, 0.5f, 1;
        mesh.add_triangle(0, 1, 2);
        mesh.add_polygon({0, 3});
        REQUIRE(mesh.is_hybrid());
        auto grid = lagrange::volume::mesh_to_volume(mesh, m2v_opt);
        REQUIRE(grid->activeVoxelCount() > 0);
        auto out =
            lagrange::volume::volume_to_mesh<lagrange::SurfaceMesh<Scalar, Index>>(*grid, v2m_opt);
        REQUIRE(out.get_num_facets() > 0);
    }
    SECTION("edges contribute to voxelization with polygonal mesh")
    {
        // Create a triangle mesh with an additional edge that extends far from the triangle.
        // If the edge is properly voxelized, the grid should have more active voxels than the
        // triangle alone. This tests the preserve_edges path during triangulation.
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(7);
        auto vertices = vertex_ref(mesh);
        vertices.row(0) << 0, 0, 0;
        vertices.row(1) << 1, 0, 0;
        vertices.row(2) << 0.5f, 1, 0;
        vertices.row(3) << 0, 0, 0;
        vertices.row(4) << 1, 0, 0;
        vertices.row(5) << 0.5f, 1, 0;
        vertices.row(6) << 0, 0, 5; // far away from triangle
        // Add a polygon (n-gon > 4) to force triangulation path
        mesh.add_polygon({0, 1, 2, 3, 4});
        mesh.add_polygon({0, 6});
        REQUIRE(mesh.is_hybrid());
        auto grid = lagrange::volume::mesh_to_volume(mesh, m2v_opt);
        REQUIRE(grid->activeVoxelCount() > 0);
        auto bbox = grid->evalActiveVoxelBoundingBox();
        auto nz = bbox.max().z() - bbox.min().z() + 1;
        // The triangle is at z=0, the edge extends to z=5.
        // With voxel_size=0.1, the z-extent should be at least 50 voxels if the edge is included.
        REQUIRE(nz > 40);
    }

    SECTION("pure point mesh")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(4);
        auto vertices = vertex_ref(mesh);
        vertices.row(0) << 0, 0, 0;
        vertices.row(1) << 1, 0, 0;
        vertices.row(2) << 0, 1, 0;
        vertices.row(3) << 0, 0, 1;
        for (uint32_t i = 0; i < 4; ++i) {
            mesh.add_polygon({i});
        }
        REQUIRE(mesh.is_regular());
        REQUIRE(mesh.get_vertex_per_facet() == 1);
        auto grid = lagrange::volume::mesh_to_volume(mesh, m2v_opt);
        REQUIRE(grid->activeVoxelCount() > 0);
    }

    SECTION("hybrid mesh with points, edges, and triangles")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(5);
        auto vertices = vertex_ref(mesh);
        vertices.row(0) << 0, 0, 0;
        vertices.row(1) << 1, 0, 0;
        vertices.row(2) << 0.5f, 1, 0;
        vertices.row(3) << 0.5f, 0.5f, 1;
        vertices.row(4) << 0, 0, 3;
        mesh.add_triangle(0, 1, 2);
        mesh.add_polygon({0, 3});
        mesh.add_polygon({4});
        REQUIRE(mesh.is_hybrid());
        auto grid = lagrange::volume::mesh_to_volume(mesh, m2v_opt);
        REQUIRE(grid->activeVoxelCount() > 0);
    }

    SECTION("points contribute to voxelization with polygonal mesh")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(7);
        auto vertices = vertex_ref(mesh);
        vertices.row(0) << 0, 0, 0;
        vertices.row(1) << 1, 0, 0;
        vertices.row(2) << 0.5f, 1, 0;
        vertices.row(3) << 0, 0, 0;
        vertices.row(4) << 1, 0, 0;
        vertices.row(5) << 0.5f, 1, 0;
        vertices.row(6) << 0, 0, 5; // far away from polygon
        // Add a polygon (n-gon > 4) to force triangulation path
        mesh.add_polygon({0, 1, 2, 3, 4});
        mesh.add_polygon({6}); // point facet
        REQUIRE(mesh.is_hybrid());
        auto grid = lagrange::volume::mesh_to_volume(mesh, m2v_opt);
        REQUIRE(grid->activeVoxelCount() > 0);
        auto bbox = grid->evalActiveVoxelBoundingBox();
        auto nz = bbox.max().z() - bbox.min().z() + 1;
        // The polygon is at z=0, the point is at z=5.
        // With voxel_size=0.1, the z-extent should be at least 50 voxels if the point is included.
        REQUIRE(nz > 40);
    }
}

TEST_CASE("mesh_to_volume: polygonal mesh", "[volume]")
{
    using Scalar = float;
    using Index = uint32_t;

    lagrange::volume::MeshToVolumeOptions m2v_opt;
    m2v_opt.signing_method = lagrange::volume::MeshToVolumeOptions::Sign::FloodFill;
    m2v_opt.voxel_size = 0.1;

    SECTION("hybrid mesh")
    {
        auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>(
            "open/core/poly/mixedFaringPart.obj");
        REQUIRE(mesh.is_hybrid());
        auto grid = lagrange::volume::mesh_to_volume(mesh, m2v_opt);
        REQUIRE(grid->activeVoxelCount() > 0);
    }

    SECTION("poly mesh")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertices(7);
        auto vertices = vertex_ref(mesh);
        for (Index i = 0; i < 7; ++i) {
            vertices.row(i) << std::cos(2 * lagrange::internal::pi * i / 7),
                std::sin(2 * lagrange::internal::pi * i / 7), 0;
        }
        mesh.add_polygon({0, 1, 2, 3, 4, 5, 6});
        REQUIRE(mesh.is_regular());
        REQUIRE(mesh.get_vertex_per_facet() == 7);
        auto grid = lagrange::volume::mesh_to_volume(mesh, m2v_opt);
        REQUIRE(grid->activeVoxelCount() > 0);
    }
}

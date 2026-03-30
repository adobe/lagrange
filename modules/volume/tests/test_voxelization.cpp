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

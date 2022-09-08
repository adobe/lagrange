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
#include <lagrange/volume/mesh_to_volume.h>
#include <lagrange/volume/volume_to_mesh.h>
#include <lagrange/testing/common.h>

TEST_CASE("voxelization: reproducibility", "[volume]")
{
    auto mesh = lagrange::create_sphere();
    auto grid = lagrange::volume::mesh_to_volume(*mesh, 0.1);
    auto grid2 = lagrange::volume::mesh_to_volume(*mesh, 0.1);
    auto mesh2 = lagrange::volume::volume_to_mesh<lagrange::TriangleMesh3Df>(*grid);
    auto mesh3 = lagrange::volume::volume_to_mesh<lagrange::TriangleMesh3Df>(*grid2);
    REQUIRE(mesh2->get_num_vertices() > 0);
    REQUIRE(mesh2->get_num_facets() > 0);
    REQUIRE(mesh2->get_vertices() == mesh3->get_vertices());
    REQUIRE(mesh2->get_facets() == mesh3->get_facets());
}

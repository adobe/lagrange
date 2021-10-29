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
#include <lagrange/thicken_and_close_mesh.h>
#include <Eigen/Core>
#include <lagrange/testing/common.h>


TEST_CASE("thicken_and_close_mesh wing", "[mesh][thicken_and_close_mesh][wing]")
{
    using namespace lagrange;

    auto mesh = lagrange::testing::load_mesh<TriangleMesh3Df>("corp/core/wing.obj");
    mesh->initialize_edge_data();
    REQUIRE(mesh->get_vertex_per_facet() == 3);

    auto thickened_mesh = thicken_and_close_mesh(*mesh, 1.f, 3);
    REQUIRE(mesh->get_num_vertices()*2 <= thickened_mesh->get_num_vertices());
}

TEST_CASE("thicken_and_close_mesh stanford-bunny", "[mesh][thicken_and_close_mesh][stanford-bunny]")
{
    using namespace lagrange;

    auto mesh = lagrange::testing::load_mesh<TriangleMesh3Df>("open/core/stanford-bunny.obj");
    mesh->initialize_edge_data();

    REQUIRE(mesh->get_vertex_per_facet() == 3);
    auto thickened_mesh = thicken_and_close_mesh(*mesh, 1.f, 3);
    REQUIRE(mesh->get_num_vertices()*2 <= thickened_mesh->get_num_vertices());
}

TEST_CASE("thicken_and_close_mesh hemisphere", "[mesh][thicken_and_close_mesh][hemisphere]")
{
    using MeshType = lagrange::TriangleMesh3D;
    auto mesh = lagrange::testing::load_mesh<MeshType>("open/core/hemisphere.obj");
    mesh->initialize_edge_data();

    Eigen::Vector3d dir(0, 1, 0);
    auto flat_mesh = lagrange::thicken_and_close_mesh(*mesh, dir, -0.5, 0);
    auto mirrored_mesh = lagrange::thicken_and_close_mesh(*mesh, dir, 0, -1);
    auto thickened_mesh = lagrange::thicken_and_close_mesh(*mesh, 1);

    REQUIRE(flat_mesh->get_num_vertices() == 682);
    REQUIRE(flat_mesh->get_num_facets() == 1360);
    REQUIRE(mirrored_mesh->get_num_vertices() == 682);
    REQUIRE(mirrored_mesh->get_num_facets() == 1360);
    REQUIRE(thickened_mesh->get_num_vertices() == 682);
    REQUIRE(thickened_mesh->get_num_facets() == 1360);
}


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
////////////////////////////////////////////////////////////////////////////////
#include <lagrange/io/load_mesh.h>
#include <lagrange/partitioning/partition_mesh_vertices.h>

#include <lagrange/testing/common.h>
#include <lagrange/Mesh.h>
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Partitioning: Reproducibility", "[partitioning]" LA_SLOW_DEBUG_FLAG)
{
    auto mesh = lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/core/bunny_simple.obj");
    REQUIRE(mesh);
    REQUIRE(mesh->get_num_vertices() == 2503);
    REQUIRE(mesh->get_num_facets() == 5002);

    for (int k : {1, 2, 4, 8, 16, 2503}) {
        auto p1 = lagrange::partitioning::partition_mesh_vertices(mesh->get_facets(), k);
        auto p2 = lagrange::partitioning::partition_mesh_vertices(mesh->get_facets(), k);
        REQUIRE(p1 == p2);
    }
}

TEST_CASE("Partitioning: Validity", "[partitioning]")
{
    auto mesh = lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/core/bunny_simple.obj");
    REQUIRE(mesh);
    REQUIRE(mesh->get_num_vertices() == 2503);
    REQUIRE(mesh->get_num_facets() == 5002);

    for (int k : {1, 2, 4, 8, 16, 2503}) {
        auto p1 = lagrange::partitioning::partition_mesh_vertices(mesh->get_facets(), k);
        REQUIRE((p1.array() >= 0).all());
        REQUIRE((p1.array() < k).all());
    }
}

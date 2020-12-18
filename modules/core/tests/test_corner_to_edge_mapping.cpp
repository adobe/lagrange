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
#include <lagrange/corner_to_edge_mapping.h>
#include <lagrange/io/load_mesh.h>


// clang-format off
#include <lagrange/utils/warnoff.h>
#include <igl/readDMAT.h>
#include <igl/writeDMAT.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <lagrange/testing/common.h>

TEST_CASE("corner_to_edge_mapping: replicability", "[core]")
{
    auto mesh = lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/core/hemisphere.obj");
    REQUIRE(mesh);
    REQUIRE(mesh->get_num_vertices() == 341);
    REQUIRE(mesh->get_num_facets() == 640);
    Eigen::VectorXi c2e_0, c2e_1;
    auto ne0 = lagrange::corner_to_edge_mapping(mesh->get_facets(), c2e_0);
    auto ne1 = lagrange::corner_to_edge_mapping(mesh->get_facets(), c2e_1);
    REQUIRE(ne0 == ne1);
    REQUIRE(ne0 == c2e_0.maxCoeff() + 1);
    REQUIRE(c2e_0.size() == mesh->get_facets().size());
    REQUIRE(c2e_0.size() == c2e_1.size());
    REQUIRE(c2e_0 == c2e_1);
}

TEST_CASE("corner_to_edge_mapping: regression", "[core]")
{
    auto mesh = lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/core/hemisphere.obj");
    REQUIRE(mesh);
    REQUIRE(mesh->get_num_vertices() == 341);
    REQUIRE(mesh->get_num_facets() == 640);
    Eigen::VectorXi c2e, c2e_ref;
    auto ne = lagrange::corner_to_edge_mapping(mesh->get_facets(), c2e);
    REQUIRE(ne == c2e.maxCoeff() + 1);
    REQUIRE(c2e.size() == mesh->get_facets().size());
    auto ret = igl::readDMAT(lagrange::testing::get_data_path("open/core/hemisphere.edges.dmat").string(), c2e_ref);
    REQUIRE(ret);
    REQUIRE(c2e.size() == c2e_ref.size());
    REQUIRE(c2e == c2e_ref);
}

/*
 * Copyright 2019 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/testing/common.h>

#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_mesh.h>

#include <catch2/catch_approx.hpp>

TEST_CASE("drop", "[mesh][io]")
{
    using namespace lagrange;
    auto mesh = lagrange::testing::load_mesh<TriangleMesh3D>("open/core/drop_tri.obj");
    lagrange::io::save_mesh("io_test_drop.obj", *mesh);
    auto mesh2 = lagrange::io::load_mesh<TriangleMesh3D>("io_test_drop.obj");
    fs::remove("io_test_drop.obj");
    REQUIRE(mesh->get_num_vertices() == mesh2->get_num_vertices());
    REQUIRE(mesh->get_num_facets() == mesh2->get_num_facets());
    REQUIRE(mesh->is_uv_initialized());
    REQUIRE(mesh2->is_uv_initialized());

    REQUIRE((mesh->get_uv() - mesh2->get_uv()).norm() == Catch::Approx(0.0).margin(1e-14));
    REQUIRE((mesh->get_uv_indices() - mesh2->get_uv_indices()).norm() == Catch::Approx(0.0));
}

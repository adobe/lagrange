/*
 * Copyright 2025 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/compute_components.h>
#include <lagrange/io/load_mesh_stl.h>
#include <lagrange/testing/check_mesh.h>
#include <lagrange/testing/common.h>
#include <lagrange/topology.h>

TEST_CASE("load_mesh_stl", "[io][stl]")
{
    const auto filepath = lagrange::testing::get_data_path("open/io/61765.stl");

    lagrange::io::LoadOptions options;
    options.stitch_vertices = true;
    auto mesh = lagrange::io::load_mesh_stl<lagrange::SurfaceMesh32d>(filepath, options);
    REQUIRE(mesh.get_num_vertices() == 21549);
    REQUIRE(mesh.get_num_facets() == 45966);
    lagrange::ComponentOptions comp_options;
    comp_options.connectivity_type = lagrange::ConnectivityType::Vertex;
    REQUIRE(lagrange::compute_components(mesh, comp_options) == 60);
    REQUIRE(lagrange::compute_components(mesh) == 61);
    REQUIRE(lagrange::compute_euler(mesh) == 1779);
}

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
#include <lagrange/testing/common.h>
#include <lagrange/io/load_mesh_gltf.h>
#include <lagrange/io/load_simple_scene_gltf.h>
#include <lagrange/attribute_names.h>

using namespace lagrange;

// this file is a single gltf with embedded buffers
TEST_CASE("load_mesh_gltf", "[io]") {
    auto mesh = io::load_mesh_gltf<lagrange::SurfaceMesh32f>(
        testing::get_data_path("open/io/three_cubes_instances.gltf"));
    REQUIRE(mesh.get_num_vertices() == 24);
    REQUIRE(mesh.get_num_facets() == 12);
    REQUIRE(mesh.has_attribute(AttributeName::normal));
    REQUIRE(mesh.has_attribute(AttributeName::texcoord));
}

TEST_CASE("load_simple_scene_gltf", "[io]") {
    auto scene = lagrange::io::load_simple_scene_gltf<lagrange::scene::SimpleScene32f3>(
        lagrange::testing::get_data_path("open/io/three_cubes_instances.gltf"));
    REQUIRE(scene.get_num_meshes() == 1);
    REQUIRE(scene.get_num_instances(0) == 3);

    const auto& t1 = scene.get_instance(0, 0).transform;
    const auto& t2 = scene.get_instance(0, 1).transform;
    const auto& t3 = scene.get_instance(0, 2).transform;
    REQUIRE(!t1.isApprox(t2));
    REQUIRE(!t2.isApprox(t3));
    REQUIRE(!t3.isApprox(t1));
    
}

// this file is a gltf with separate .bin and textures
TEST_CASE("load_mesh_gltf_animated_cube", "[io]") {
    auto mesh = io::load_mesh_gltf<lagrange::SurfaceMesh32f>(
        testing::get_data_path("open/io/gltf_animated_cube/AnimatedCube.gltf"));
    REQUIRE(mesh.get_num_vertices() == 36);
    REQUIRE(mesh.get_num_facets() == 12);
    REQUIRE(mesh.has_attribute(AttributeName::normal));
    REQUIRE(mesh.has_attribute(AttributeName::texcoord));

}


// this file contains a single mesh with two separate components.
TEST_CASE("load_gltf_avocado", "[io]") {
    auto mesh = io::load_mesh_gltf<lagrange::SurfaceMesh32f>(
        testing::get_data_path("open/io/gltf_avocado/Avocado.gltf"));
    REQUIRE(mesh.get_num_vertices() > 0);
    REQUIRE(mesh.get_num_facets() > 0);
    REQUIRE(mesh.has_attribute(AttributeName::normal));
    REQUIRE(mesh.has_attribute(AttributeName::texcoord));

    auto scene = io::load_simple_scene_gltf<lagrange::scene::SimpleScene32f3>(
        testing::get_data_path("open/io/gltf_avocado/Avocado.gltf"));
    REQUIRE(scene.get_num_meshes() == 1);
    REQUIRE(scene.get_num_instances(0) == 1);
}

// this file contains a model made of many different meshes (29!). 
// There are no textures and no UVs, each component has a material with a different base color.
// note that this file has a total of 10413 degenerate triangles out of 75730. 
TEST_CASE("load_gltf_engine", "[io]") {
    auto mesh = io::load_mesh_gltf<lagrange::SurfaceMesh32f>(
        testing::get_data_path("open/io/gltf_engine/2CylinderEngine.gltf"));
    REQUIRE(mesh.get_num_vertices() == 55843);
    REQUIRE(mesh.get_num_facets() == 75730);
    REQUIRE(mesh.has_attribute(AttributeName::normal));
}

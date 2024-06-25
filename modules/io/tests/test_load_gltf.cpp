/*
 * Copyright 2023 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/attribute_names.h>
#include <lagrange/io/load_mesh_gltf.h>
#include <lagrange/io/load_simple_scene_gltf.h>
#include <lagrange/io/save_simple_scene_gltf.h>
#include <lagrange/mesh_cleanup/remove_topologically_degenerate_facets.h>
#include <lagrange/scene/simple_scene_convert.h>
#include <lagrange/testing/common.h>

using namespace lagrange;

// this file is a single gltf with embedded buffers
TEST_CASE("load_mesh_gltf", "[io]")
{
    SECTION("stitched")
    {
        lagrange::io::LoadOptions options;
        options.stitch_vertices = true;
        auto mesh = io::load_mesh_gltf<lagrange::SurfaceMesh32f>(
            testing::get_data_path("open/io/three_cubes_instances.gltf"),
            options);
        REQUIRE(mesh.get_num_vertices() == 3 * 8);
        REQUIRE(mesh.get_num_facets() == 3 * 12);
        REQUIRE(mesh.has_attribute(AttributeName::normal));
        REQUIRE(mesh.has_attribute(std::string(AttributeName::texcoord) + "_0"));
    }

    SECTION("unstitched")
    {
        lagrange::io::LoadOptions options;
        options.stitch_vertices = false;
        auto mesh = io::load_mesh_gltf<lagrange::SurfaceMesh32f>(
            testing::get_data_path("open/io/three_cubes_instances.gltf"),
            options);
        REQUIRE(mesh.get_num_vertices() == 3 * 24);
        REQUIRE(mesh.get_num_facets() == 3 * 12);
        REQUIRE(mesh.has_attribute(AttributeName::normal));
        REQUIRE(mesh.has_attribute(std::string(AttributeName::texcoord) + "_0"));
    }
}

TEST_CASE("load_simple_scene_gltf", "[io]")
{
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
TEST_CASE("load_mesh_gltf_animated_cube", "[io]")
{
    auto mesh = io::load_mesh_gltf<lagrange::SurfaceMesh32f>(
        testing::get_data_path("open/io/gltf_animated_cube/AnimatedCube.gltf"));
    REQUIRE(mesh.get_num_vertices() == 36);
    REQUIRE(mesh.get_num_facets() == 12);
    REQUIRE(mesh.has_attribute(AttributeName::normal));
    REQUIRE(mesh.has_attribute(std::string(AttributeName::texcoord) + "_0"));
}

// this file contains a single mesh with two separate components.
TEST_CASE("load_gltf_avocado", "[io]")
{
    auto mesh = io::load_mesh_gltf<lagrange::SurfaceMesh32f>(
        testing::get_data_path("open/io/avocado/Avocado.gltf"));
    REQUIRE(mesh.get_num_vertices() > 0);
    REQUIRE(mesh.get_num_facets() > 0);
    REQUIRE(mesh.has_attribute(AttributeName::normal));
    REQUIRE(mesh.has_attribute(std::string(AttributeName::texcoord) + "_0"));

    auto scene = io::load_simple_scene_gltf<lagrange::scene::SimpleScene32f3>(
        testing::get_data_path("open/io/avocado/Avocado.gltf"));
    REQUIRE(scene.get_num_meshes() == 1);
    REQUIRE(scene.get_num_instances(0) == 1);
}

// this file contains a single mesh with two texcoords.
TEST_CASE("load_gltf_multi_uv", "[io]")
{
    auto mesh = io::load_mesh_gltf<lagrange::SurfaceMesh32f>(
        testing::get_data_path("open/io/MultiUVTest.glb"));
    REQUIRE(mesh.get_num_vertices() > 0);
    REQUIRE(mesh.get_num_facets() > 0);
    REQUIRE(mesh.has_attribute(AttributeName::normal));
    REQUIRE(mesh.has_attribute(std::string(AttributeName::texcoord) + "_0"));
    REQUIRE(mesh.has_attribute(std::string(AttributeName::texcoord) + "_1"));

    auto scene = io::load_simple_scene_gltf<lagrange::scene::SimpleScene32f3>(
        testing::get_data_path("open/io/MultiUVTest.glb"));
    REQUIRE(scene.get_num_meshes() == 1);
    REQUIRE(scene.get_num_instances(0) == 1);
}

// This file contains a model made of many different meshes (29!)
// There are no textures and no UVs, each component has a material with a different base color.
// There are a number of topologically degenerate facets, which are removed by Blender if you try to
// use it to export the scene to .obj.
TEST_CASE("load_gltf_engine", "[io]")
{
    // Load as single mesh
    auto mesh = io::load_mesh_gltf<lagrange::SurfaceMesh32f>(
        testing::get_data_path("open/io/gltf_engine/2CylinderEngine.gltf"));
    REQUIRE(mesh.get_num_vertices() == 84657);
    REQUIRE(mesh.get_num_facets() == 121496);
    REQUIRE(mesh.has_attribute(AttributeName::normal));

    // Load as simple scene
    using Index = uint32_t;
    auto scene = io::load_simple_scene_gltf<lagrange::scene::SimpleScene32f3>(
        testing::get_data_path("open/io/gltf_engine/2CylinderEngine.gltf"));
    Index num_facets = 0;
    for (Index i = 0; i < scene.get_num_meshes(); i++) {
        lagrange::remove_topologically_degenerate_facets(scene.ref_mesh(i));
        num_facets += scene.get_mesh(i).get_num_facets() * scene.get_num_instances(i);
    }
    REQUIRE(num_facets == 110342); // Matches the #facets on a .obj exported from Blender
}

TEST_CASE("load_glb_triangle", "[io]")
{
    auto mesh = io::load_mesh_gltf<lagrange::SurfaceMesh32f>(
        testing::get_data_path("open/io/triangle.glb"));
    REQUIRE(mesh.get_num_vertices() == 3);
    REQUIRE(mesh.get_num_facets() == 1);
}

TEST_CASE("load_gltf_point_cloud", "[io][gltf]")
{
    using Scalar = double;
    using Index = uint32_t;

    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});

    std::stringstream ss;
    auto scene = lagrange::scene::mesh_to_simple_scene(mesh);
    REQUIRE(scene.get_num_meshes() == 1);
    lagrange::io::save_simple_scene_gltf(ss, scene);
    auto scene2 =
        lagrange::io::load_simple_scene_gltf<lagrange::scene::SimpleScene<Scalar, Index>>(ss);
    REQUIRE(scene2.get_num_meshes() == 1);
}

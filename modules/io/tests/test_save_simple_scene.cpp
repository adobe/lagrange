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
#include <lagrange/io/save_simple_scene.h>
#include <lagrange/io/load_simple_scene.h>
#include <lagrange/testing/common.h>
#include <lagrange/testing/create_test_mesh.h>

#include <sstream>

using namespace lagrange;

scene::SimpleScene32d3 create_simple_scene()
{
    auto cube = testing::create_test_cube<double, uint32_t>();
    auto sphere = testing::create_test_sphere<double, uint32_t>();

    scene::SimpleScene32d3 scene;
    auto cube_idx = scene.add_mesh(std::move(cube));
    auto sphere_idx = scene.add_mesh(std::move(sphere));

    using AffineTransform = typename decltype(scene)::AffineTransform;
    AffineTransform t1 = AffineTransform::Identity();
    AffineTransform t2 = AffineTransform::Identity();
    AffineTransform t3 = AffineTransform::Identity();
    t1.translate(Eigen::Vector3d(0, -3, 0));
    t2.translate(Eigen::Vector3d(3, 0, 0));
    t3.translate(Eigen::Vector3d(-3, 0, 0));
    scene.add_instance({cube_idx, t1});
    scene.add_instance({cube_idx, t2});
    scene.add_instance({sphere_idx, t3});

    return scene;
}

TEST_CASE("save_simple_scene", "[io]")
{
    auto scene = create_simple_scene();
    
    SECTION("Export with materials (default)")
    {
        std::stringstream ss;
        io::SaveOptions opt;
        opt.encoding = io::FileEncoding::Ascii;
        
        REQUIRE_NOTHROW(io::save_simple_scene(ss, scene, io::FileFormat::Gltf, opt));
        
        // Load the scene back and verify round-trip
        auto loaded_scene = io::load_simple_scene<scene::SimpleScene32d3>(ss);
        
        // Verify scene structure is preserved
        REQUIRE(loaded_scene.get_num_meshes() == scene.get_num_meshes());
        
        // Verify each mesh data integrity
        for (uint32_t i = 0; i < scene.get_num_meshes(); ++i) {
            const auto& original_mesh = scene.get_mesh(i);
            const auto& loaded_mesh = loaded_scene.get_mesh(i);
            
            REQUIRE(loaded_mesh.get_num_vertices() == original_mesh.get_num_vertices());
            REQUIRE(loaded_mesh.get_num_facets() == original_mesh.get_num_facets());
            
            // Verify we have the same number of instances for this mesh
            REQUIRE(loaded_scene.get_num_instances(i) == scene.get_num_instances(i));
        }
        
        // Verify total number of instances across all meshes
        size_t total_original_instances = 0;
        size_t total_loaded_instances = 0;
        for (uint32_t i = 0; i < scene.get_num_meshes(); ++i) {
            total_original_instances += scene.get_num_instances(i);
            total_loaded_instances += loaded_scene.get_num_instances(i);
        }
        REQUIRE(total_loaded_instances == total_original_instances);
    }
    
    SECTION("Export without materials")
    {
        std::stringstream ss;
        io::SaveOptions opt_no_materials;
        opt_no_materials.encoding = io::FileEncoding::Ascii;
        opt_no_materials.export_materials = false;
        
        REQUIRE_NOTHROW(io::save_simple_scene(ss, scene, io::FileFormat::Gltf, opt_no_materials));
        
        // Load the scene back and verify round-trip
        auto loaded_scene = io::load_simple_scene<scene::SimpleScene32d3>(ss);
        
        // Verify scene structure is preserved (should be identical to default case)
        REQUIRE(loaded_scene.get_num_meshes() == scene.get_num_meshes());
        
        // Verify each mesh data integrity
        for (uint32_t i = 0; i < scene.get_num_meshes(); ++i) {
            const auto& original_mesh = scene.get_mesh(i);
            const auto& loaded_mesh = loaded_scene.get_mesh(i);
            
            REQUIRE(loaded_mesh.get_num_vertices() == original_mesh.get_num_vertices());
            REQUIRE(loaded_mesh.get_num_facets() == original_mesh.get_num_facets());
            
            // Verify we have the same number of instances for this mesh
            REQUIRE(loaded_scene.get_num_instances(i) == scene.get_num_instances(i));
        }
        
        // Verify total number of instances across all meshes
        size_t total_original_instances = 0;
        size_t total_loaded_instances = 0;
        for (uint32_t i = 0; i < scene.get_num_meshes(); ++i) {
            total_original_instances += scene.get_num_instances(i);
            total_loaded_instances += loaded_scene.get_num_instances(i);
        }
        REQUIRE(total_loaded_instances == total_original_instances);
    }
}

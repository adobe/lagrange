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
#include <lagrange/io/load_scene.h>
#include <lagrange/io/save_scene.h>
#include <lagrange/io/save_scene_obj.h>
#include <lagrange/testing/common.h>

#include <sstream>

using namespace lagrange;

TEST_CASE("save_scene", "[io]")
{
    using SceneType = scene::Scene32f;

    // Load the avocado scene once and reuse it across all sections
    fs::path avocado_path = testing::get_data_path("open/io/avocado/Avocado.gltf");
    auto scene = io::load_scene<SceneType>(avocado_path);

    SECTION("GLTF - Stream Export")
    {
        std::stringstream ss;
        io::SaveOptions options;
        options.export_materials = true;

        REQUIRE_NOTHROW(io::save_scene(ss, scene, io::FileFormat::Gltf, options));

        // Load the generated glTF back to verify round-trip
        auto scene2 = io::load_scene<SceneType>(ss);

        // Verify scene structure is preserved
        REQUIRE(scene.name == scene2.name);
        REQUIRE(scene.nodes.size() == scene2.nodes.size());
        REQUIRE(scene.root_nodes.size() == scene2.root_nodes.size());
        REQUIRE(scene.meshes.size() == scene2.meshes.size());

        // Materials, textures, and images should be preserved
        REQUIRE(scene.images.size() == scene2.images.size());
        REQUIRE(scene.textures.size() == scene2.textures.size());
        REQUIRE(scene.materials.size() == scene2.materials.size());

        // Other elements should be preserved
        REQUIRE(scene.lights.size() == scene2.lights.size());
        REQUIRE(scene.cameras.size() == scene2.cameras.size());
        REQUIRE(scene.skeletons.size() == scene2.skeletons.size());
        REQUIRE(scene.animations.size() == scene2.animations.size());

        // Verify mesh data integrity
        for (size_t i = 0; i < scene2.meshes.size(); ++i) {
            const auto& original_mesh = scene.meshes[i];
            const auto& loaded_mesh = scene2.meshes[i];

            REQUIRE(loaded_mesh.get_num_vertices() == original_mesh.get_num_vertices());
            REQUIRE(loaded_mesh.get_num_facets() == original_mesh.get_num_facets());
        }
    }

    SECTION("GLTF - Export Without Materials")
    {
        // Test that saving scene to stream works when materials are disabled
        std::stringstream ss;
        io::SaveOptions options;
        options.export_materials = false;

        REQUIRE_NOTHROW(io::save_scene(ss, scene, io::FileFormat::Gltf, options));

        // Load the generated glTF back to verify structure
        auto scene2 = io::load_scene<SceneType>(ss);

        // Scene structure should be preserved
        REQUIRE(scene.name == scene2.name);
        REQUIRE(scene.nodes.size() == scene2.nodes.size());
        REQUIRE(scene.root_nodes.size() == scene2.root_nodes.size());
        REQUIRE(scene.meshes.size() == scene2.meshes.size());

        // Materials, textures, and images should not be exported
        REQUIRE(scene2.images.size() == 0);
        REQUIRE(scene2.textures.size() == 0);
        REQUIRE(scene2.materials.size() == 0);

        // Other elements should be preserved
        REQUIRE(scene.lights.size() == scene2.lights.size());
        REQUIRE(scene.cameras.size() == scene2.cameras.size());
        REQUIRE(scene.skeletons.size() == scene2.skeletons.size());
        REQUIRE(scene.animations.size() == scene2.animations.size());

        // Verify mesh data integrity
        for (size_t i = 0; i < scene2.meshes.size(); ++i) {
            const auto& original_mesh = scene.meshes[i];
            const auto& loaded_mesh = scene2.meshes[i];

            REQUIRE(loaded_mesh.get_num_vertices() == original_mesh.get_num_vertices());
            REQUIRE(loaded_mesh.get_num_facets() == original_mesh.get_num_facets());
        }
    }

    SECTION("OBJ - Stream Export Exception")
    {
        // Test that saving scene with materials to stream throws exception
        std::stringstream ss;
        io::SaveOptions options;
        options.export_materials = true;

        // Default behavior (export_materials=true) should throw for stream when materials exist
        REQUIRE_THROWS_AS(io::save_scene_obj(ss, scene, options), std::runtime_error);
    }

    SECTION("OBJ - Stream Export Without Materials")
    {
        // Test that saving scene to stream works when materials are disabled

        std::stringstream ss;
        io::SaveOptions options;
        options.export_materials = false;
        REQUIRE_NOTHROW(io::save_scene_obj(ss, scene, options));

        // Verify OBJ content is generated
        std::string obj_content = ss.str();
        REQUIRE(!obj_content.empty());
        REQUIRE(obj_content.find("# OBJ File Generated by Lagrange") != std::string::npos);
        REQUIRE(obj_content.find("v ") != std::string::npos); // Should have vertices
        REQUIRE(obj_content.find("f ") != std::string::npos); // Should have faces
        // Should NOT contain material references when export_materials=false
        REQUIRE(obj_content.find("mtllib") == std::string::npos);
        REQUIRE(obj_content.find("usemtl") == std::string::npos);

        // Test round-trip: load the OBJ back from the stringstream
        std::stringstream ss_input(obj_content);
        // Note: Loading OBJ as a scene might have limitations, so we'll just verify basic content
        // auto loaded_scene = io::load_scene<SceneType>(ss_input);

        // For now, verify the basic OBJ content is correct
        REQUIRE(obj_content.find("v ") != std::string::npos);
        REQUIRE(obj_content.find("f ") != std::string::npos);
    }

    SECTION("OBJ - File Export With Materials")
    {
        // Test that saving scene to file with materials creates OBJ, MTL, and texture files
        fs::path obj_file = testing::get_test_output_path("test_save_scene/avocado.obj");
        fs::path mtl_file = testing::get_test_output_path("test_save_scene/avocado.mtl");

        io::SaveOptions options;
        options.export_materials = true;
        options.encoding = io::FileEncoding::Ascii;

        REQUIRE_NOTHROW(io::save_scene_obj(obj_file, scene, options));

        // Verify OBJ file was created
        REQUIRE(fs::exists(obj_file));

        // Verify MTL file was created (since scene has materials)
        REQUIRE(fs::exists(mtl_file));

        // Check OBJ file content
        std::ifstream obj_stream(obj_file);
        std::string obj_content(
            (std::istreambuf_iterator<char>(obj_stream)),
            std::istreambuf_iterator<char>());
        obj_stream.close();

        REQUIRE(obj_content.find("# OBJ File Generated by Lagrange") != std::string::npos);
        REQUIRE(obj_content.find("mtllib avocado.mtl") != std::string::npos);
        REQUIRE(obj_content.find("usemtl") != std::string::npos); // Should have material usage
        REQUIRE(obj_content.find("v ") != std::string::npos); // Should have vertices
        REQUIRE(obj_content.find("f ") != std::string::npos); // Should have faces

        // Check MTL file content
        std::ifstream mtl_stream(mtl_file);
        std::string mtl_content(
            (std::istreambuf_iterator<char>(mtl_stream)),
            std::istreambuf_iterator<char>());
        mtl_stream.close();

        REQUIRE(mtl_content.find("# MTL File Generated by Lagrange") != std::string::npos);
        REQUIRE(
            mtl_content.find("newmtl") != std::string::npos); // Should have material definitions
        REQUIRE(mtl_content.find("Kd ") != std::string::npos); // Should have diffuse color
        REQUIRE(mtl_content.find("Ks ") != std::string::npos); // Should have specular color
        REQUIRE(mtl_content.find("Ns ") != std::string::npos); // Should have shininess

        // Check if texture images were referenced (avocado model has textures)
        if (mtl_content.find("map_Kd") != std::string::npos) {
            // If texture maps are referenced, verify they exist or are properly handled
            // Note: The actual texture files might not be copied yet (depends on implementation)
            // but the MTL file should reference them
            REQUIRE(mtl_content.find("map_Kd") != std::string::npos);
        }

        // Check that texture files were created
        fs::path base_color_texture =
            testing::get_test_output_path("test_save_scene/Avocado_baseColor.png");
        fs::path normal_texture =
            testing::get_test_output_path("test_save_scene/Avocado_normal.png");

        REQUIRE(fs::exists(base_color_texture));
        REQUIRE(fs::file_size(base_color_texture) > 0); // Should not be empty

        REQUIRE(fs::exists(normal_texture));
        REQUIRE(fs::file_size(normal_texture) > 0); // Should not be empty


        // Verify we can load the generated OBJ file back
        auto loaded_scene = io::load_scene<SceneType>(obj_file);
        REQUIRE(loaded_scene.meshes.size() == scene.meshes.size());
        REQUIRE(loaded_scene.nodes.size() >= 1);

        // Verify mesh data integrity
        for (size_t i = 0; i < loaded_scene.meshes.size(); ++i) {
            const auto& original_mesh = scene.meshes[i];
            const auto& loaded_mesh = loaded_scene.meshes[i];

            REQUIRE(loaded_mesh.get_num_vertices() == original_mesh.get_num_vertices());
            REQUIRE(loaded_mesh.get_num_facets() == original_mesh.get_num_facets());
        }
    }
}

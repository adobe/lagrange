/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/Attribute.h>
#include <lagrange/attribute_names.h>
#include <lagrange/io/load_mesh_gltf.h>
#include <lagrange/io/load_scene_gltf.h>
#include <lagrange/io/load_simple_scene_gltf.h>
#include <lagrange/io/save_mesh_gltf.h>
#include <lagrange/io/save_scene_gltf.h>
#include <lagrange/io/save_simple_scene_gltf.h>
#include <lagrange/scene/SceneTypes.h>
#include <lagrange/scene/SimpleSceneTypes.h>
#include <lagrange/testing/common.h>

#include <catch2/generators/catch_generators.hpp>

#include <sstream>

using namespace lagrange;

TEST_CASE("gltf mesh names with instances", "[io][gltf]")
{
    const auto path = testing::get_data_path("open/io/three_cubes_instances.gltf");
    const std::string name = "Cube";
    io::SaveOptions options;
    options.encoding = GENERATE(io::FileEncoding::Ascii, io::FileEncoding::Binary);

    SECTION("mesh")
    {
        auto mesh = io::load_mesh_gltf<SurfaceMesh32f>(path);
        REQUIRE(mesh.get_num_facets() == 36);
        REQUIRE(mesh.get_metadata(AttributeName::name) == name);

        std::stringstream output;
        io::save_mesh_gltf(output, mesh, options);
        auto reloaded = io::load_mesh_gltf<SurfaceMesh32f>(output);
        REQUIRE(reloaded.get_metadata(AttributeName::name) == name);
    }
    SECTION("simple scene")
    {
        auto scene = io::load_simple_scene_gltf<scene::SimpleScene32f3>(path);
        REQUIRE(scene.get_num_meshes() == 1);
        REQUIRE(scene.get_num_instances(0) == 3);
        REQUIRE(scene.get_mesh(0).get_num_facets() == 12);
        REQUIRE(scene.get_mesh(0).get_metadata(AttributeName::name) == name);

        std::stringstream output;
        io::save_simple_scene_gltf(output, scene, options);
        auto reloaded = io::load_simple_scene_gltf<scene::SimpleScene32f3>(output);
        REQUIRE(reloaded.get_mesh(0).get_metadata(AttributeName::name) == name);
    }
    SECTION("scene")
    {
        auto scene = io::load_scene_gltf<scene::Scene32f>(path);
        REQUIRE(scene.meshes.size() == 1);
        REQUIRE(scene.meshes[0].get_metadata(AttributeName::name) == name);
        REQUIRE(scene.nodes[0].name == "Cube");
        REQUIRE(scene.nodes[1].name == "Cube.001");
        REQUIRE(scene.nodes[2].name == "Cube.002");

        std::stringstream output;
        io::save_scene_gltf(output, scene, options);
        auto reloaded = io::load_scene_gltf<scene::Scene32f>(output);
        REQUIRE(reloaded.meshes.size() == 3);
        for (const auto& mesh : reloaded.meshes) {
            REQUIRE(mesh.get_metadata(AttributeName::name) == name);
        }
        for (size_t i = 0; i < scene.nodes.size(); ++i) {
            REQUIRE(reloaded.nodes[i].name == scene.nodes[i].name);
        }
    }
}

TEST_CASE("gltf mesh names with multiple primitives", "[io][gltf]")
{
    const auto path = testing::get_data_path("open/io/gltf_engine/2CylinderEngine.gltf");
    const std::string name = "Piston_123-844_0_Parts_1";
    auto simple_scene = io::load_simple_scene_gltf<scene::SimpleScene32f3>(path);
    REQUIRE(simple_scene.get_mesh(0).get_metadata(AttributeName::name) == name);
    auto scene = io::load_scene_gltf<scene::Scene32f>(path);
    REQUIRE(scene.meshes[0].get_metadata(AttributeName::name) == name);
    REQUIRE(scene.meshes[1].get_metadata(AttributeName::name) == name);
}

TEST_CASE("gltf mesh name export", "[io][gltf]")
{
    SurfaceMesh32f mesh;
    mesh.add_vertices(3, {0, 0, 0, 1, 0, 0, 0, 1, 0});
    mesh.add_triangle(0, 1, 2);
    const std::string name = "Triangle \"with quotes\" / café";
    io::SaveOptions options;
    options.encoding = GENERATE(io::FileEncoding::Ascii, io::FileEncoding::Binary);
    std::string expected_name;

    SECTION("named")
    {
        mesh.create_metadata(AttributeName::name, name);
        expected_name = name;
    }
    SECTION("unnamed") {}
    SECTION("unrelated metadata without name")
    {
        mesh.create_metadata("author", "Author");
    }
    SECTION("unrelated metadata before name")
    {
        mesh.create_metadata("author", "Author");
        mesh.create_metadata(AttributeName::name, name);
        expected_name = name;
    }
    SECTION("empty name")
    {
        mesh.create_metadata(AttributeName::name, "");
    }
    SECTION("name independent of attribute selection")
    {
        mesh.create_metadata(AttributeName::name, name);
        options.output_attributes = io::SaveOptions::OutputAttributes::SelectedOnly;
        expected_name = name;
    }
    SECTION("numeric attribute named name")
    {
        const float values[] = {0, 1, 2};
        mesh.create_attribute<float>(
            "name",
            AttributeElement::Vertex,
            AttributeUsage::Scalar,
            1,
            values);
    }
    SECTION("indexed string attribute named name")
    {
        const uint8_t values[] = {'A', 'B', 'C'};
        const uint32_t indices[] = {0, 1, 2};
        mesh.create_attribute<uint8_t>(
            AttributeName::name,
            AttributeElement::Indexed,
            AttributeUsage::String,
            1,
            values,
            indices);
    }
    SECTION("indexed attribute conversion")
    {
        mesh.create_metadata(AttributeName::name, name);
        const float values[] = {0, 0, 1, 0, 0, 1};
        const uint32_t indices[] = {0, 1, 2};
        mesh.create_attribute<float>(
            "uv",
            AttributeElement::Indexed,
            AttributeUsage::UV,
            2,
            values,
            indices);
        options.attribute_conversion_policy =
            io::SaveOptions::AttributeConversionPolicy::ConvertAsNeeded;
        expected_name = name;
    }
    SECTION("triangulation")
    {
        mesh.clear_facets();
        mesh.add_vertex({1, 1, 0});
        mesh.add_quad(0, 1, 3, 2);
        mesh.create_metadata(AttributeName::name, name);
        expected_name = name;
    }

    std::stringstream output;
    io::save_mesh_gltf(output, mesh, options);
    auto reloaded = io::load_mesh_gltf<SurfaceMesh32f>(output);
    if (expected_name.empty()) {
        REQUIRE_FALSE(reloaded.has_attribute(AttributeName::name));
    } else {
        REQUIRE(reloaded.get_metadata(AttributeName::name) == expected_name);
        REQUIRE_FALSE(reloaded.has_attribute("_name"));
        const auto& attr = reloaded.get_attribute<uint8_t>(AttributeName::name);
        REQUIRE(attr.get_element_type() == AttributeElement::Value);
        REQUIRE(attr.get_usage() == AttributeUsage::String);
        REQUIRE(attr.get_num_channels() == 1);
    }
}

TEST_CASE("gltf distinct mesh names", "[io][gltf]")
{
    SurfaceMesh32f mesh;
    mesh.add_vertices(3, {0, 0, 0, 1, 0, 0, 0, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.create_metadata(AttributeName::name, "First mesh");
    auto other = mesh;
    other.set_metadata(AttributeName::name, "Second mesh");
    io::SaveOptions options;
    options.encoding = GENERATE(io::FileEncoding::Ascii, io::FileEncoding::Binary);
    scene::SimpleScene32f3 scene;
    scene.add_instance({scene.add_mesh(mesh)});
    scene.add_instance({scene.add_mesh(other)});
    std::stringstream output;
    io::save_simple_scene_gltf(output, scene, options);

    std::stringstream simple_input(output.str());
    auto simple_scene = io::load_simple_scene_gltf<scene::SimpleScene32f3>(simple_input);
    std::stringstream full_input(output.str());
    auto full_scene = io::load_scene_gltf<scene::Scene32f>(full_input);
    REQUIRE(simple_scene.get_num_meshes() == 2);
    REQUIRE(full_scene.meshes.size() == 2);
    for (uint32_t i = 0; i < 2; ++i) {
        const auto name = scene.get_mesh(i).get_metadata(AttributeName::name);
        REQUIRE(simple_scene.get_mesh(i).get_metadata(AttributeName::name) == name);
        REQUIRE(full_scene.meshes[i].get_metadata(AttributeName::name) == name);
    }
    auto flattened = io::load_mesh_gltf<SurfaceMesh32f>(output);
    REQUIRE(flattened.get_num_facets() == 2);
    REQUIRE_FALSE(flattened.has_attribute(AttributeName::name));
}

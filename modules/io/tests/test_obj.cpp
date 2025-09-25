/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/io/load_mesh_obj.h>
#include <lagrange/io/save_mesh_obj.h>
#include <lagrange/io/save_scene_obj.h>
#include <lagrange/io/save_simple_scene_obj.h>
#include <lagrange/testing/check_mesh.h>
#include <lagrange/testing/common.h>
#include <lagrange/testing/create_test_mesh.h>
#include <lagrange/testing/equivalence_check.h>

#include <catch2/catch_approx.hpp>

TEST_CASE("Grenade_H", "[mesh][io]" LA_CORP_FLAG)
{
    using namespace lagrange;
    auto mesh = io::load_mesh_obj<SurfaceMesh32d>(testing::get_data_path("corp/io/Grenade_H.obj"));
}

TEST_CASE("io/obj", "[io][obj]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = testing::create_test_sphere<Scalar, Index>();
    std::stringstream data;
    io::SaveOptions save_options;
    save_options.output_attributes = io::SaveOptions::OutputAttributes::All;
    save_options.attribute_conversion_policy =
        io::SaveOptions::AttributeConversionPolicy::ConvertAsNeeded;

    save_options.encoding = io::FileEncoding::Ascii;
    REQUIRE_NOTHROW(io::save_mesh_obj(data, mesh, save_options));
    auto mesh2 = io::load_mesh_obj<SurfaceMesh<Scalar, Index>>(data);
    testing::check_mesh(mesh2);
    testing::ensure_approx_equivalent_mesh(mesh, mesh2);
}

TEST_CASE("io/obj empty", "[io][obj]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh;
    std::stringstream data;
    io::SaveOptions save_options;
    save_options.output_attributes = io::SaveOptions::OutputAttributes::All;
    save_options.attribute_conversion_policy =
        io::SaveOptions::AttributeConversionPolicy::ConvertAsNeeded;

    save_options.encoding = io::FileEncoding::Ascii;
    REQUIRE_NOTHROW(io::save_mesh_obj(data, mesh, save_options));
    auto mesh2 = io::load_mesh_obj<SurfaceMesh<Scalar, Index>>(data);
    testing::check_mesh(mesh2);
    testing::ensure_approx_equivalent_mesh(mesh, mesh2);
}

TEST_CASE("io/obj simple_scene", "[io][obj]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    // Create a simple scene with two meshes and multiple instances
    auto cube = testing::create_test_cube<Scalar, Index>();
    auto sphere = testing::create_test_sphere<Scalar, Index>();

    scene::SimpleScene<Scalar, Index, 3> scene;
    auto cube_idx = scene.add_mesh(std::move(cube));
    auto sphere_idx = scene.add_mesh(std::move(sphere));

    // Add some instances with transforms
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

    // Save to OBJ format
    std::stringstream data;
    io::SaveOptions save_options;
    save_options.encoding = io::FileEncoding::Ascii;
    REQUIRE_NOTHROW(io::save_simple_scene_obj(data, scene, save_options));

    // Check that the output contains some expected content
    std::string output = data.str();
    REQUIRE(!output.empty());
    REQUIRE(output.find("v ") != std::string::npos); // Should have vertices
    REQUIRE(output.find("f ") != std::string::npos); // Should have faces
}

TEST_CASE("io/obj scene", "[io][obj]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    // Create a scene with nodes and mesh instances
    scene::Scene<Scalar, Index> scene;
    scene.name = "Test Scene";

    // Add a test mesh
    auto cube = testing::create_test_cube<Scalar, Index>();
    scene.meshes.push_back(std::move(cube));

    // Create a root node
    scene::Node root_node;
    root_node.name = "Root";
    root_node.transform = Eigen::Affine3f::Identity();
    scene.nodes.push_back(root_node);
    scene.root_nodes.push_back(0);

    // Create a child node with mesh instance
    scene::Node child_node;
    child_node.name = "Child";
    child_node.transform = Eigen::Affine3f::Identity();
    child_node.transform.translate(Eigen::Vector3f(2, 0, 0));
    child_node.parent = 0;

    scene::SceneMeshInstance mesh_instance;
    mesh_instance.mesh = 0;
    child_node.meshes.push_back(mesh_instance);

    scene.nodes.push_back(child_node);
    scene.nodes[0].children.push_back(1);

    // Save to OBJ format
    std::stringstream data;
    io::SaveOptions save_options;
    save_options.encoding = io::FileEncoding::Ascii;
    REQUIRE_NOTHROW(io::save_scene_obj(data, scene, save_options));

    // Check that the output contains some expected content
    std::string output = data.str();
    REQUIRE(!output.empty());
    REQUIRE(output.find("v ") != std::string::npos); // Should have vertices
    REQUIRE(output.find("f ") != std::string::npos); // Should have faces
}

TEST_CASE("io/obj scene with materials", "[io][obj]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    // Create a scene with nodes and mesh instances
    scene::Scene<Scalar, Index> scene;
    scene.name = "Test Scene with Materials";

    // Add a test mesh
    auto cube = testing::create_test_cube<Scalar, Index>();
    scene.meshes.push_back(std::move(cube));

    // Create a material
    scene::MaterialExperimental material;
    material.name = "test_material";
    material.base_color_value = Eigen::Vector4f(1.0f, 0.0f, 0.0f, 1.0f); // Red
    scene.materials.push_back(material);

    // Create a root node
    scene::Node root_node;
    root_node.name = "Root";
    root_node.transform = Eigen::Affine3f::Identity();
    scene.nodes.push_back(root_node);
    scene.root_nodes.push_back(0);

    // Create a child node with mesh instance
    scene::Node child_node;
    child_node.name = "Child";
    child_node.transform = Eigen::Affine3f::Identity();
    child_node.transform.translate(Eigen::Vector3f(2, 0, 0));
    child_node.parent = 0;

    scene::SceneMeshInstance mesh_instance;
    mesh_instance.mesh = 0;
    mesh_instance.materials.push_back(0); // Use the material we created
    child_node.meshes.push_back(mesh_instance);

    scene.nodes.push_back(child_node);
    scene.nodes[0].children.push_back(1);

    // Test 1: Save to stream with materials should throw an exception
    {
        std::stringstream data;
        io::SaveOptions save_options;
        save_options.encoding = io::FileEncoding::Ascii;
        save_options.export_materials = true;
        REQUIRE_THROWS_WITH(
            io::save_scene_obj(data, scene, save_options),
            "Cannot export materials when saving to stream. Use file-based save_scene_obj() instead or set export_materials=false."
        );
    }

    // Test 1b: Save to stream without materials flag should work
    {
        std::stringstream data;
        io::SaveOptions save_options;
        save_options.encoding = io::FileEncoding::Ascii;
        save_options.export_materials = false;
        REQUIRE_NOTHROW(io::save_scene_obj(data, scene, save_options));

        std::string output = data.str();
        REQUIRE(!output.empty());
        REQUIRE(output.find("v ") != std::string::npos); // Should have vertices
        REQUIRE(output.find("f ") != std::string::npos); // Should have faces
        // Should NOT contain mtllib or usemtl when materials are not exported
        REQUIRE(output.find("mtllib") == std::string::npos);
        REQUIRE(output.find("usemtl") == std::string::npos);
    }

    // Test 2: Save to file with materials should create MTL file
    {
        fs::path obj_file = testing::get_test_output_path("test_obj/test_with_materials.obj");
        fs::path mtl_file = testing::get_test_output_path("test_obj/test_with_materials.mtl");

        io::SaveOptions save_options;
        save_options.encoding = io::FileEncoding::Ascii;
        save_options.export_materials = true;
        REQUIRE_NOTHROW(io::save_scene_obj(obj_file, scene, save_options));

        // Check that OBJ file was created
        REQUIRE(fs::exists(obj_file));

        // Check that MTL file was created
        REQUIRE(fs::exists(mtl_file));

        // Check OBJ file content
        std::ifstream obj_stream(obj_file);
        std::string obj_content((std::istreambuf_iterator<char>(obj_stream)),
                                std::istreambuf_iterator<char>());
        obj_stream.close();
        REQUIRE(obj_content.find("mtllib test_with_materials.mtl") != std::string::npos);
        REQUIRE(obj_content.find("usemtl test_material") != std::string::npos);

        // Check MTL file content
        std::ifstream mtl_stream(mtl_file);
        std::string mtl_content((std::istreambuf_iterator<char>(mtl_stream)),
                                std::istreambuf_iterator<char>());
        mtl_stream.close();
        REQUIRE(mtl_content.find("newmtl test_material") != std::string::npos);
        REQUIRE(mtl_content.find("Kd 1 0 0") != std::string::npos); // Red diffuse color
    }
}

TEST_CASE("io/obj 2d mesh", "[io][obj]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    // Create a simple 2D triangle mesh
    SurfaceMesh<Scalar, Index> mesh(2);
    mesh.add_vertex({0.0, 0.0});
    mesh.add_vertex({1.0, 0.0});
    mesh.add_vertex({0.5, 1.0});
    mesh.add_triangle(0, 1, 2);

    // Add a 2D UV attribute
    auto uv_id = mesh.template create_attribute<Scalar>(
        "uv",
        lagrange::AttributeElement::Vertex,
        lagrange::AttributeUsage::UV,
        2);
    auto uv_attr = lagrange::attribute_matrix_ref<Scalar>(mesh, uv_id);
    uv_attr.row(0) << 0.0, 0.0;
    uv_attr.row(1) << 1.0, 0.0;
    uv_attr.row(2) << 0.5, 1.0;

    // Test saving to stream
    std::stringstream data;
    io::SaveOptions save_options;
    save_options.output_attributes = io::SaveOptions::OutputAttributes::All;
    save_options.encoding = io::FileEncoding::Ascii;

    REQUIRE_NOTHROW(io::save_mesh_obj(data, mesh, save_options));

    std::string output = data.str();
    REQUIRE(!output.empty());

    // Check that we have 2D vertices (only x and y coordinates)
    REQUIRE(output.find("v 0 0") != std::string::npos);
    REQUIRE(output.find("v 1 0") != std::string::npos);
    REQUIRE(output.find("v 0.5 1") != std::string::npos);

    // Check that we have UV coordinates
    REQUIRE(output.find("vt ") != std::string::npos);

    // Check that we have faces
    REQUIRE(output.find("f ") != std::string::npos);

    // Test loading the saved mesh back
    auto mesh2 = io::load_mesh_obj<SurfaceMesh<Scalar, Index>>(data);
    testing::check_mesh(mesh2);

    // The loaded mesh should be 3D (OBJ format enforces 3D), but should have the same connectivity
    REQUIRE(mesh2.get_dimension() == 3);
    REQUIRE(mesh2.get_num_vertices() == 3);
    REQUIRE(mesh2.get_num_facets() == 1);

    // Check that the 2D coordinates were preserved in x,y and z=0
    auto vertices = lagrange::vertex_view(mesh2);
    REQUIRE(vertices(0, 0) == Catch::Approx(0.0));
    REQUIRE(vertices(0, 1) == Catch::Approx(0.0));
    REQUIRE(vertices(0, 2) == Catch::Approx(0.0));
    REQUIRE(vertices(1, 0) == Catch::Approx(1.0));
    REQUIRE(vertices(1, 1) == Catch::Approx(0.0));
    REQUIRE(vertices(1, 2) == Catch::Approx(0.0));
    REQUIRE(vertices(2, 0) == Catch::Approx(0.5));
    REQUIRE(vertices(2, 1) == Catch::Approx(1.0));
    REQUIRE(vertices(2, 2) == Catch::Approx(0.0));
}

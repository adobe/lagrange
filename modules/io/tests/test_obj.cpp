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
#include <lagrange/views.h>

#include <lagrange/attribute_names.h>
#include <lagrange/views.h>

#include <catch2/catch_approx.hpp>

#include <algorithm>

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
            "Cannot export materials when saving to stream. Use file-based save_scene_obj() "
            "instead or set export_materials=false.");
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
        std::string obj_content(
            (std::istreambuf_iterator<char>(obj_stream)),
            std::istreambuf_iterator<char>());
        obj_stream.close();
        REQUIRE(obj_content.find("mtllib test_with_materials.mtl") != std::string::npos);
        REQUIRE(obj_content.find("usemtl test_material") != std::string::npos);

        // Check MTL file content
        std::ifstream mtl_stream(mtl_file);
        std::string mtl_content(
            (std::istreambuf_iterator<char>(mtl_stream)),
            std::istreambuf_iterator<char>());
        mtl_stream.close();
        REQUIRE(mtl_content.find("newmtl test_material") != std::string::npos);
        REQUIRE(mtl_content.find("Kd 1 0 0") != std::string::npos); // Red diffuse color
    }
}

TEST_CASE("io/obj missing indices", "[io][obj]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    // OBJ with 4 vertices, 3 UV coords, 1 normal, and 3 object groups.
    // Each group uses a different face format and includes one face with
    // indices and one without, to test missing index handling.
    // tinyobjloader requires consistent face formats within a single group,
    // so we use separate "o" groups to mix formats.
    const std::string obj_data = R"(v 0 0 0
v 1 0 0
v 0 1 0
v 1 1 0
vt 0.0 0.0
vt 1.0 0.0
vt 0.0 1.0
vn 0 0 1
o uv_only
f 1/1 2/2 3/3
f 3 2 4
o normal_only
f 1//1 2//1 3//1
f 3 2 4
o all
f 1/1/1 2/2/1 3/3/1
f 3 2 4
)";

    std::istringstream ss(obj_data);
    auto mesh = io::load_mesh_obj<SurfaceMesh<Scalar, Index>>(ss);
    REQUIRE(mesh.get_num_facets() == 6);
    REQUIRE(mesh.has_attribute("texcoord"));
    REQUIRE(mesh.has_attribute("normal"));

    constexpr Index num_uv_values = 3;
    constexpr Index num_nrm_values = 1;

    auto& uv_attr = mesh.get_indexed_attribute<Scalar>("texcoord");
    auto uv_indices = uv_attr.indices().get_all();
    auto& nrm_attr = mesh.get_indexed_attribute<Scalar>("normal");
    auto nrm_indices = nrm_attr.indices().get_all();

    REQUIRE(mesh.get_vertex_per_facet() == 3);
    const Index nvpf = 3;

    // Check that all corners of a face have original (non-remapped) indices.
    auto check_valid = [&](const auto& indices, Index face_index, Index original_count) {
        for (Index c = 0; c < nvpf; ++c) {
            CHECK(indices[face_index * nvpf + c] < original_count);
        }
    };

    // Check that all corners of a face have been remapped to appended default values.
    auto check_remapped = [&](const auto& indices, Index face_index, Index original_count) {
        for (Index c = 0; c < nvpf; ++c) {
            CHECK(indices[face_index * nvpf + c] != invalid<Index>());
            CHECK(indices[face_index * nvpf + c] >= original_count);
        }
    };

    // uv_only group: face 0 has UVs, face 1 has neither.
    check_valid(uv_indices, 0, num_uv_values);
    check_remapped(nrm_indices, 0, num_nrm_values);
    check_remapped(uv_indices, 1, num_uv_values);
    check_remapped(nrm_indices, 1, num_nrm_values);

    // normal_only group: face 2 has normals, face 3 has neither.
    check_remapped(uv_indices, 2, num_uv_values);
    check_valid(nrm_indices, 2, num_nrm_values);
    check_remapped(uv_indices, 3, num_uv_values);
    check_remapped(nrm_indices, 3, num_nrm_values);

    // all group: face 4 has both, face 5 has neither.
    check_valid(uv_indices, 4, num_uv_values);
    check_valid(nrm_indices, 4, num_nrm_values);
    check_remapped(uv_indices, 5, num_uv_values);
    check_remapped(nrm_indices, 5, num_nrm_values);
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

    // Check that 2D vertices are written with an explicit z=0 coordinate
    REQUIRE(output.find("v 0 0 0\n") != std::string::npos);
    REQUIRE(output.find("v 1 0 0\n") != std::string::npos);
    REQUIRE(output.find("v 0.5 1 0\n") != std::string::npos);

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

TEST_CASE("io/obj stitch_vertices welds indexed attributes", "[io][obj]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    // OBJ with two triangles sharing an edge, but with duplicated UV values along the seam.
    // Vertices 2 and 4 are at the same position (1 0 0); vertices 3 and 5 are at the same
    // position (0 1 0).
    // UV coords 2 and 4 are identical (1 0); UV coords 3 and 5 are identical (0 1).
    // After stitch_vertices, vertices should be merged AND the duplicate UV values should be
    // welded.
    const std::string obj_data = R"(v 0 0 0
v 1 0 0
v 0 1 0
v 1 0 0
v 0 1 0
v 1 1 0
vt 0 0
vt 1 0
vt 0 1
vt 1 0
vt 0 1
vt 1 1
f 1/1 2/2 3/3
f 4/4 6/6 5/5
)";

    io::LoadOptions load_options;
    load_options.stitch_vertices = true;

    std::istringstream ss(obj_data);
    auto mesh = io::load_mesh_obj<SurfaceMesh<Scalar, Index>>(ss, load_options);

    // After stitching, duplicate vertices should be merged: 6 -> 4
    CHECK(mesh.get_num_vertices() == 4);
    CHECK(mesh.get_num_facets() == 2);

    // The UV attribute should have duplicate values welded: 6 -> 4
    REQUIRE(mesh.has_attribute("texcoord"));
    auto& uv_attr = mesh.get_indexed_attribute<Scalar>("texcoord");
    CHECK(uv_attr.values().get_num_elements() == 4);
}

TEST_CASE("io/obj line elements", "[io][obj]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;
    using MeshType = SurfaceMesh<Scalar, Index>;

    // OBJ with 2 triangles, 1 polyline (3 vertices = 2 segments), and 1 edge (2 vertices)
    std::string obj_data = R"(
v 0 0 0
v 1 0 0
v 1 1 0
v 0 1 0
v 2 0 0
v 3 0 0
v 4 0 0
v 5 0 0
v 6 0 0
f 1 2 3
f 1 3 4
l 5 6 7
l 8 9
)";

    SECTION("load line elements")
    {
        std::istringstream input(obj_data);
        auto mesh = io::load_mesh_obj<MeshType>(input);
        testing::check_mesh(mesh);

        // 2 face facets + 2 line segments (polyline 5-6-7) + 1 line segment (edge 8-9) = 5
        REQUIRE(mesh.get_num_vertices() == 9);
        REQUIRE(mesh.get_num_facets() == 5);

        // Check face facets have 3 vertices
        REQUIRE(mesh.get_facet_size(0) == 3);
        REQUIRE(mesh.get_facet_size(1) == 3);
        // Check line segments have 2 vertices
        REQUIRE(mesh.get_facet_size(2) == 2);
        REQUIRE(mesh.get_facet_size(3) == 2);
        REQUIRE(mesh.get_facet_size(4) == 2);

        // Check line_id attribute exists
        REQUIRE(mesh.has_attribute(AttributeName::line_id));
        auto lid = mesh.get_attribute_id(AttributeName::line_id);
        const auto& line_id_attr = mesh.get_attribute<Index>(lid);
        auto line_ids = line_id_attr.get_all();

        // Face facets have line_id == 0
        REQUIRE(line_ids[0] == 0);
        REQUIRE(line_ids[1] == 0);
        // Polyline "l 5 6 7" produces 2 segments, both with line_id == 1
        REQUIRE(line_ids[2] == 1);
        REQUIRE(line_ids[3] == 1);
        // Edge "l 8 9" produces 1 segment with line_id == 2
        REQUIRE(line_ids[4] == 2);

        // Verify line segment vertex connectivity
        // Segment from polyline: 5-6 (0-indexed: 4-5)
        auto seg0 = mesh.get_facet_vertices(2);
        REQUIRE(seg0[0] == 4); // vertex 5 (0-indexed)
        REQUIRE(seg0[1] == 5); // vertex 6 (0-indexed)
        // Segment from polyline: 6-7 (0-indexed: 5-6)
        auto seg1 = mesh.get_facet_vertices(3);
        REQUIRE(seg1[0] == 5);
        REQUIRE(seg1[1] == 6);
        // Segment from edge: 8-9 (0-indexed: 7-8)
        auto seg2 = mesh.get_facet_vertices(4);
        REQUIRE(seg2[0] == 7);
        REQUIRE(seg2[1] == 8);
    }

    SECTION("load_lines=false skips line elements")
    {
        std::istringstream input(obj_data);
        io::LoadOptions load_options;
        load_options.load_lines = false;
        auto mesh = io::load_mesh_obj<MeshType>(input, load_options);
        testing::check_mesh(mesh);

        // Only the 2 face facets remain; the 3 line segments are skipped
        REQUIRE(mesh.get_num_vertices() == 9);
        REQUIRE(mesh.get_num_facets() == 2);
        REQUIRE(mesh.get_facet_size(0) == 3);
        REQUIRE(mesh.get_facet_size(1) == 3);

        // No line_id attribute is created when lines are not loaded
        REQUIRE_FALSE(mesh.has_attribute(AttributeName::line_id));
    }

    SECTION("roundtrip preserves polylines")
    {
        std::istringstream input(obj_data);
        auto mesh = io::load_mesh_obj<MeshType>(input);

        // Save to OBJ
        std::stringstream output;
        io::SaveOptions save_options;
        io::save_mesh_obj(output, mesh, save_options);
        std::string saved = output.str();

        // Output should contain face lines
        REQUIRE(saved.find("f ") != std::string::npos);
        // Output should contain line directives
        REQUIRE(saved.find("l ") != std::string::npos);

        // The polyline "l 5 6 7" should be saved as a single "l" command with 3 vertices
        // (not split into two separate "l" commands)
        // Find all "l " lines
        std::vector<std::string> l_lines;
        std::istringstream line_reader(saved);
        std::string line;
        while (std::getline(line_reader, line)) {
            if (line.size() >= 2 && line[0] == 'l' && line[1] == ' ') {
                l_lines.push_back(line);
            }
        }
        REQUIRE(l_lines.size() == 2); // one polyline + one edge

        // Count tokens in each line directive
        auto count_tokens = [](const std::string& s) {
            std::istringstream iss(s);
            std::string token;
            int count = 0;
            while (iss >> token) count++;
            return count;
        };
        // Expect one polyline with 3 vertices and one edge with 2 vertices
        std::vector<int> counts;
        for (auto& l : l_lines) counts.push_back(count_tokens(l));
        std::sort(counts.begin(), counts.end());
        REQUIRE(counts == std::vector<int>{3, 4});

        // Reload and verify same topology
        std::istringstream reload_input(saved);
        auto mesh2 = io::load_mesh_obj<MeshType>(reload_input);
        testing::check_mesh(mesh2);
        REQUIRE(mesh2.get_num_vertices() == mesh.get_num_vertices());
        REQUIRE(mesh2.get_num_facets() == mesh.get_num_facets());
        REQUIRE(mesh2.has_attribute(AttributeName::line_id));
    }

    SECTION("faces only produces no line_id attribute")
    {
        std::string faces_only = R"(
v 0 0 0
v 1 0 0
v 1 1 0
f 1 2 3
)";
        std::istringstream input(faces_only);
        auto mesh = io::load_mesh_obj<MeshType>(input);
        REQUIRE(mesh.get_num_facets() == 1);
        REQUIRE_FALSE(mesh.has_attribute(AttributeName::line_id));
    }

    SECTION("multiple shapes with lines")
    {
        // Two groups, each with faces and lines. Tests that facet ordering and
        // ref_middle offsets are correct when line segments follow all faces.
        std::string multi_shape = R"(
v 0 0 0
v 1 0 0
v 1 1 0
v 0 1 0
v 2 0 0
v 3 0 0
v 4 0 0
v 5 0 0
g group1
f 1 2 3
l 5 6
g group2
f 1 3 4
l 7 8
)";
        std::istringstream input(multi_shape);
        io::LoadOptions load_options;
        load_options.load_object_ids = true;
        auto mesh = io::load_mesh_obj<MeshType>(input, load_options);
        testing::check_mesh(mesh);

        // 2 face facets + 2 line segments = 4 total facets
        REQUIRE(mesh.get_num_facets() == 4);
        // Faces come first (size 3), then line segments (size 2)
        REQUIRE(mesh.get_facet_size(0) == 3);
        REQUIRE(mesh.get_facet_size(1) == 3);
        REQUIRE(mesh.get_facet_size(2) == 2);
        REQUIRE(mesh.get_facet_size(3) == 2);

        // Check line_id attribute
        REQUIRE(mesh.has_attribute(AttributeName::line_id));
        auto lid = mesh.get_attribute_id(AttributeName::line_id);
        const auto& line_id_attr = mesh.get_attribute<Index>(lid);
        auto line_ids = line_id_attr.get_all();
        REQUIRE(line_ids[0] == 0); // face from group1
        REQUIRE(line_ids[1] == 0); // face from group2
        REQUIRE(line_ids[2] == 1); // line from group1
        REQUIRE(line_ids[3] == 2); // line from group2

        // Check object_id attribute: face and line from same group share same id
        REQUIRE(mesh.has_attribute(AttributeName::object_id));
        auto oid = mesh.get_attribute_id(AttributeName::object_id);
        const auto& object_id_attr = mesh.get_attribute<Index>(oid);
        auto object_ids = object_id_attr.get_all();
        REQUIRE(object_ids[0] == 0); // face from group1
        REQUIRE(object_ids[1] == 1); // face from group2
        REQUIRE(object_ids[2] == 0); // line from group1
        REQUIRE(object_ids[3] == 1); // line from group2

        // Verify vertex connectivity of line segments
        auto seg0 = mesh.get_facet_vertices(2);
        REQUIRE(seg0[0] == 4); // v5 0-indexed
        REQUIRE(seg0[1] == 5); // v6 0-indexed
        auto seg1 = mesh.get_facet_vertices(3);
        REQUIRE(seg1[0] == 6); // v7 0-indexed
        REQUIRE(seg1[1] == 7); // v8 0-indexed

        // Roundtrip should preserve topology
        std::stringstream output;
        io::save_mesh_obj(output, mesh);
        std::istringstream reload_input(output.str());
        auto mesh2 = io::load_mesh_obj<MeshType>(reload_input);
        testing::check_mesh(mesh2);
        REQUIRE(mesh2.get_num_facets() == mesh.get_num_facets());
        REQUIRE(mesh2.get_num_vertices() == mesh.get_num_vertices());
    }

    SECTION("closed loop polyline roundtrip")
    {
        // A triangle (closed loop) represented as line segments.
        // Tests close_loop_with_identical_vertices in the saver.
        std::string loop_data = R"(
v 0 0 0
v 1 0 0
v 0.5 1 0
l 1 2 3 1
)";
        std::istringstream input(loop_data);
        auto mesh = io::load_mesh_obj<MeshType>(input);
        testing::check_mesh(mesh);

        // 3 line segments from the closed polyline "l 1 2 3 1"
        REQUIRE(mesh.get_num_facets() == 3);
        for (Index f = 0; f < 3; ++f) {
            REQUIRE(mesh.get_facet_size(f) == 2);
        }

        // Save and reload
        std::stringstream output;
        io::save_mesh_obj(output, mesh);
        std::string saved = output.str();

        // Should produce a single "l" directive with 4 tokens (closing vertex repeated)
        std::vector<std::string> l_lines;
        std::istringstream line_reader(saved);
        std::string line;
        while (std::getline(line_reader, line)) {
            if (line.size() >= 2 && line[0] == 'l' && line[1] == ' ') {
                l_lines.push_back(line);
            }
        }
        REQUIRE(l_lines.size() == 1);

        // Parse the vertex indices from the "l" line
        std::istringstream iss(l_lines[0]);
        std::string tok;
        iss >> tok; // skip "l"
        std::vector<int> verts;
        while (iss >> tok) verts.push_back(std::stoi(tok));
        // A closed loop should have 4 indices with first == last
        REQUIRE(verts.size() == 4);
        REQUIRE(verts.front() == verts.back());

        // Reload and verify same facet count
        std::istringstream reload_input(saved);
        auto mesh2 = io::load_mesh_obj<MeshType>(reload_input);
        testing::check_mesh(mesh2);
        REQUIRE(mesh2.get_num_facets() == mesh.get_num_facets());
    }

    SECTION("deterministic output order")
    {
        // Multiple polylines should always be emitted in ascending line_id order.
        std::istringstream input(obj_data);
        auto mesh = io::load_mesh_obj<MeshType>(input);

        // Save twice and compare
        std::stringstream out1, out2;
        io::save_mesh_obj(out1, mesh);
        io::save_mesh_obj(out2, mesh);
        REQUIRE(out1.str() == out2.str());

        // Verify "l" directives appear in ascending vertex order
        std::vector<std::string> l_lines;
        std::istringstream reader(out1.str());
        std::string line;
        while (std::getline(reader, line)) {
            if (line.size() >= 2 && line[0] == 'l' && line[1] == ' ') {
                l_lines.push_back(line);
            }
        }
        REQUIRE(l_lines.size() == 2);
        // line_id 1 (polyline 5-6-7) should come before line_id 2 (edge 8-9)
        // Parse first vertex of each line
        auto first_vertex = [](const std::string& s) {
            std::istringstream iss(s);
            std::string tok;
            iss >> tok; // skip "l"
            iss >> tok;
            return std::stoi(tok);
        };
        REQUIRE(first_vertex(l_lines[0]) < first_vertex(l_lines[1]));
    }

    SECTION("saver ignores wrong line_id attribute type")
    {
        // Create a mesh with a line_id attribute of the wrong type (Scalar instead of Index).
        // The saver should ignore it and emit all facets as faces.
        MeshType mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);

        // Create a line_id attribute with float type (wrong)
        mesh.template create_attribute<Scalar>(
            AttributeName::line_id,
            AttributeElement::Facet,
            AttributeUsage::Scalar);

        std::stringstream output;
        REQUIRE_NOTHROW(io::save_mesh_obj(output, mesh));
        std::string saved = output.str();

        // Should contain a face but no line directives
        REQUIRE(saved.find("f ") != std::string::npos);
        REQUIRE(saved.find("l ") == std::string::npos);
    }

    SECTION("saver skips non-segment facets with nonzero line_id")
    {
        // Create a mesh with a triangle that has nonzero line_id.
        // The saver should warn and skip it for line output.
        MeshType mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({2, 0, 0});
        mesh.add_vertex({3, 0, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 1, 2); // second triangle, will get nonzero line_id

        auto lid = mesh.template create_attribute<Index>(
            AttributeName::line_id,
            AttributeElement::Facet,
            AttributeUsage::Scalar);
        auto& line_id_attr = mesh.template ref_attribute<Index>(lid);
        line_id_attr.ref_all()[0] = 0; // regular face
        line_id_attr.ref_all()[1] = 1; // erroneously tagged as line

        std::stringstream output;
        REQUIRE_NOTHROW(io::save_mesh_obj(output, mesh));
        std::string saved = output.str();

        // Only the first face should appear as "f", and no "l" directives
        // (the triangle with line_id=1 is skipped for face AND line output)
        int f_count = 0;
        std::istringstream reader(saved);
        std::string line;
        while (std::getline(reader, line)) {
            if (line.size() >= 2 && line[0] == 'f' && line[1] == ' ') ++f_count;
        }
        REQUIRE(f_count == 1);
        REQUIRE(saved.find("l ") == std::string::npos);
    }

    SECTION("line elements with texcoords")
    {
        // OBJ with texcoords on both faces and line elements.
        // Tests that UV indices are preserved for line segments during loading
        // and that v/vt format is used during saving.
        std::string obj_with_uv = R"(
v 0 0 0
v 1 0 0
v 1 1 0
v 2 0 0
v 3 0 0
v 4 0 0
vt 0 0
vt 1 0
vt 1 1
vt 0.5 0
vt 0.5 1
vt 0.75 0.5
f 1/1 2/2 3/3
l 4/4 5/5 6/6
)";
        std::istringstream input(obj_with_uv);
        auto mesh = io::load_mesh_obj<MeshType>(input);
        testing::check_mesh(mesh);

        // 1 face + 2 line segments (polyline 4-5-6) = 3 facets
        REQUIRE(mesh.get_num_facets() == 3);
        REQUIRE(mesh.get_facet_size(0) == 3);
        REQUIRE(mesh.get_facet_size(1) == 2);
        REQUIRE(mesh.get_facet_size(2) == 2);

        // Check that UV attribute exists
        REQUIRE(mesh.has_attribute(AttributeName::texcoord));

        // Verify line segment UV indices are valid (0-indexed: 3, 4, 5)
        auto uv_id = mesh.get_attribute_id(AttributeName::texcoord);
        const auto& uv_attr = mesh.get_indexed_attribute<Scalar>(uv_id);
        auto uv_indices = uv_attr.indices().get_all();
        // First line segment: corners of facet 1
        Index c0 = mesh.get_facet_corner_begin(1);
        REQUIRE(uv_indices[c0] == 3);
        REQUIRE(uv_indices[c0 + 1] == 4);
        // Second line segment: corners of facet 2
        Index c1 = mesh.get_facet_corner_begin(2);
        REQUIRE(uv_indices[c1] == 4);
        REQUIRE(uv_indices[c1 + 1] == 5);

        // Save and verify v/vt syntax is used for line elements
        std::stringstream output;
        io::SaveOptions save_options;
        save_options.output_attributes = io::SaveOptions::OutputAttributes::All;
        save_options.attribute_conversion_policy =
            io::SaveOptions::AttributeConversionPolicy::ConvertAsNeeded;
        io::save_mesh_obj(output, mesh, save_options);
        std::string saved = output.str();

        // Find the "l" line and verify it uses v/vt format
        std::vector<std::string> l_lines;
        std::istringstream reader(saved);
        std::string line;
        while (std::getline(reader, line)) {
            if (line.size() >= 2 && line[0] == 'l' && line[1] == ' ') {
                l_lines.push_back(line);
            }
        }
        REQUIRE(l_lines.size() == 1);
        // Should contain '/' for texcoord indices
        REQUIRE(l_lines[0].find('/') != std::string::npos);

        // Roundtrip: reload and verify UVs are preserved
        std::istringstream reload_input(saved);
        auto mesh2 = io::load_mesh_obj<MeshType>(reload_input);
        testing::check_mesh(mesh2);
        REQUIRE(mesh2.get_num_facets() == mesh.get_num_facets());
        REQUIRE(mesh2.has_attribute(AttributeName::texcoord));
        REQUIRE(mesh2.has_attribute(AttributeName::line_id));

        // Verify UV values match on the line segment corners
        auto uv_id2 = mesh2.get_attribute_id(AttributeName::texcoord);
        const auto& uv_attr2 = mesh2.get_indexed_attribute<Scalar>(uv_id2);
        auto uv_values2 = uv_attr2.values().get_all();
        auto uv_indices2 = uv_attr2.indices().get_all();
        // First line segment of reloaded mesh
        Index rc0 = mesh2.get_facet_corner_begin(1);
        Index rc1 = mesh2.get_facet_corner_begin(2);
        // UV values at these indices should match original (0.5,0), (0.5,1), (0.75,0.5)
        auto check_uv = [&](Index idx, Scalar u, Scalar v) {
            REQUIRE(uv_values2[2 * idx] == Catch::Approx(u));
            REQUIRE(uv_values2[2 * idx + 1] == Catch::Approx(v));
        };
        check_uv(uv_indices2[rc0], 0.5, 0.0);
        check_uv(uv_indices2[rc0 + 1], 0.5, 1.0);
        check_uv(uv_indices2[rc1], 0.5, 1.0);
        check_uv(uv_indices2[rc1 + 1], 0.75, 0.5);
    }

    SECTION("faces with UVs but lines without UVs")
    {
        // Mixed case: faces have texcoords but line elements don't.
        // Line elements will still use v/vt format (pointing to infinity UV values
        // created by set_invalid_indexed_values). This is acceptable.
        std::string mixed_uv = R"(
v 0 0 0
v 1 0 0
v 1 1 0
v 2 0 0
v 3 0 0
vt 0 0
vt 1 0
vt 1 1
f 1/1 2/2 3/3
l 4 5
)";
        std::istringstream input(mixed_uv);
        auto mesh = io::load_mesh_obj<MeshType>(input);
        testing::check_mesh(mesh);

        // 1 face + 1 line segment = 2 facets
        REQUIRE(mesh.get_num_facets() == 2);
        REQUIRE(mesh.has_attribute(AttributeName::line_id));

        // Save to OBJ
        std::stringstream output;
        io::SaveOptions save_options;
        save_options.output_attributes = io::SaveOptions::OutputAttributes::All;
        save_options.attribute_conversion_policy =
            io::SaveOptions::AttributeConversionPolicy::ConvertAsNeeded;
        io::save_mesh_obj(output, mesh, save_options);
        std::string saved = output.str();

        // Find face and line directives
        std::vector<std::string> f_lines, l_lines;
        std::istringstream reader(saved);
        std::string line;
        while (std::getline(reader, line)) {
            if (line.size() >= 2 && line[0] == 'f' && line[1] == ' ') f_lines.push_back(line);
            if (line.size() >= 2 && line[0] == 'l' && line[1] == ' ') l_lines.push_back(line);
        }
        REQUIRE(f_lines.size() == 1);
        REQUIRE(l_lines.size() == 1);

        // Both face and line use v/vt format since mesh has UVs globally
        REQUIRE(f_lines[0].find('/') != std::string::npos);
        REQUIRE(l_lines[0].find('/') != std::string::npos);

        // Roundtrip should preserve topology
        std::istringstream reload_input(saved);
        auto mesh2 = io::load_mesh_obj<MeshType>(reload_input);
        testing::check_mesh(mesh2);
        REQUIRE(mesh2.get_num_facets() == mesh.get_num_facets());
        REQUIRE(mesh2.has_attribute(AttributeName::line_id));
    }
}

/*
 * Copyright 2022 Adobe. All rights reserved.
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

#include <lagrange/scene/Scene.h>
#include <lagrange/scene/SimpleScene.h>
#include <lagrange/scene/scene_convert.h>
#include <lagrange/scene/scene_utils.h>
#include <lagrange/views.h>

#include <Eigen/Geometry>

#include <random>

TEST_CASE("scene_extension_value", "[scene]")
{
    using Value = lagrange::scene::Value;
    STATIC_CHECK(Value::variant_index<bool>() == 0);
    STATIC_CHECK(Value::variant_index<int>() == 1);
    STATIC_CHECK(Value::variant_index<double>() == 2);
    STATIC_CHECK(Value::variant_index<std::string>() == 3);
    STATIC_CHECK(Value::variant_index<Value::Buffer>() == 4);
    STATIC_CHECK(Value::variant_index<Value::Array>() == 5);
    STATIC_CHECK(Value::variant_index<Value::Object>() == 6);
    // float is not in the variant
    STATIC_CHECK(Value::variant_index<float>() == 7);
    STATIC_CHECK(std::variant_size_v<Value::variant_type> == 7);

    STATIC_CHECK(Value::is_variant_type<bool>());
    STATIC_CHECK(Value::is_variant_type<int>());
    STATIC_CHECK(Value::is_variant_type<double>());
    STATIC_CHECK(Value::is_variant_type<std::string>());
    STATIC_CHECK(Value::is_variant_type<Value::Buffer>());
    STATIC_CHECK(Value::is_variant_type<Value::Array>());
    STATIC_CHECK(!Value::is_variant_type<std::any>());

    Value bool_value = Value(true);
    REQUIRE(bool_value.is_bool());
    REQUIRE(!bool_value.is_number());
    REQUIRE(bool_value.get<bool>());

    Value bool_value_copy = bool_value;
    REQUIRE(bool_value_copy.is_bool());

    Value int_value = Value(123);
    REQUIRE(!int_value.is_real());
    REQUIRE(int_value.is_int());
    REQUIRE(int_value.is_number());
    REQUIRE(int_value.get<int>() == 123);

    Value int_value_copy = int_value;
    REQUIRE(int_value_copy.is_int());
    REQUIRE(int_value_copy.get_int() == 123);

    Value real_value = Value(123.4);
    REQUIRE(real_value.is_real());
    REQUIRE(real_value.is_number());

    Value string_value = Value("hello");
    REQUIRE(string_value.is_string());
    REQUIRE(string_value.get_string() == "hello");

    Value string_value_copy = string_value;
    REQUIRE(string_value_copy.get_string() == "hello");

    Value buffer_value = Value::create_buffer();
    REQUIRE(buffer_value.is_buffer());
    REQUIRE(buffer_value.size() == 0);

    Value buffer_value_copy = buffer_value;

    Value array_value = Value(Value::Array{bool_value, int_value});
    array_value.get_array().push_back(string_value);
    REQUIRE(array_value.is_array());
    REQUIRE(array_value.size() == 3);
    REQUIRE(array_value[0].get_bool() == true);
    REQUIRE(array_value[1].get_int() == 123);

    Value array_value_copy = array_value;


    Value object_value = Value::create_object();
    REQUIRE(object_value.is_object());
    REQUIRE(object_value.size() == 0);
    object_value.get_object().insert({"array", array_value});
    object_value.get_object().insert({"number", real_value});
    object_value.get_object().insert({"string", string_value});
    REQUIRE(object_value.size() == 3);
    REQUIRE(object_value["array"].is_array());
    REQUIRE(object_value["number"].get_real() == 123.4);
    REQUIRE(object_value["string"].get_string() == "hello");
}

TEST_CASE("Scene: convert", "[scene]")
{
    using Scalar = double;
    using Index = uint32_t;

    // Create dummy mesh
    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(10);
    vertex_ref(mesh).setRandom();
    mesh.add_triangles(10);
    std::mt19937 rng;
    std::uniform_int_distribution<Index> dist(0, 9);
    facet_ref(mesh).unaryExpr([&](auto) { return dist(rng); });

    auto mesh2 = lagrange::scene::scene_to_mesh(lagrange::scene::mesh_to_scene(mesh));
    REQUIRE(vertex_view(mesh) == vertex_view(mesh2));
    REQUIRE(facet_view(mesh) == facet_view(mesh2));
}

TEST_CASE("Scene: scene_to_simple_scene empty", "[scene]")
{
    using Scalar = double;
    using Index = uint32_t;

    lagrange::scene::Scene<Scalar, Index> scene;
    lagrange::scene::Node root;
    root.name = "root";
    auto root_id = scene.add(std::move(root));
    scene.root_nodes.push_back(root_id);

    auto simple = lagrange::scene::scene_to_simple_scene(scene);
    REQUIRE(simple.get_num_meshes() == 0);
}

TEST_CASE("Scene: scene_to_simple_scene basic", "[scene]")
{
    using Scalar = double;
    using Index = uint32_t;
    using namespace lagrange::scene;

    // Create a triangle mesh.
    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(3);
    auto V = lagrange::vertex_ref(mesh);
    V.row(0) << 0.0, 0.0, 0.0;
    V.row(1) << 1.0, 0.0, 0.0;
    V.row(2) << 0.0, 1.0, 0.0;
    mesh.add_triangle(0, 1, 2);

    // Build a scene with one root and one child that has a translation transform.
    Scene<Scalar, Index> scene;
    auto mesh_id = scene.add(mesh);

    Node root;
    root.name = "root";
    auto root_id = scene.add(std::move(root));
    scene.root_nodes.push_back(root_id);

    Eigen::Affine3f child_transform = Eigen::Affine3f::Identity();
    child_transform.translate(Eigen::Vector3f(1.0f, 2.0f, 3.0f));

    Node child;
    child.name = "child";
    child.transform = child_transform;
    child.parent = root_id;

    SceneMeshInstance mi;
    mi.mesh = mesh_id;
    child.meshes.push_back(std::move(mi));

    auto child_id = scene.add(std::move(child));
    scene.nodes[root_id].children.push_back(child_id);

    // Convert to SimpleScene.
    auto simple = scene_to_simple_scene(scene);

    REQUIRE(simple.get_num_meshes() == 1);
    REQUIRE(simple.get_num_instances(0) == 1);

    const auto& instance = simple.get_instance(0, 0);
    REQUIRE(instance.mesh_index == 0);

    // Check that the world transform matches the child transform (parent is identity).
    auto expected_transform = child_transform.cast<Scalar>();
    REQUIRE(instance.transform.matrix().isApprox(expected_transform.matrix(), 1e-6));
}

TEST_CASE("Scene: scene_to_simple_scene hierarchy", "[scene]")
{
    using Scalar = double;
    using Index = uint32_t;
    using namespace lagrange::scene;

    // Create a simple mesh.
    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(3);
    auto V = lagrange::vertex_ref(mesh);
    V.row(0) << 0.0, 0.0, 0.0;
    V.row(1) << 1.0, 0.0, 0.0;
    V.row(2) << 0.0, 1.0, 0.0;
    mesh.add_triangle(0, 1, 2);

    // Build a scene: root -> parent -> child (mesh instance).
    // root has translation (1,0,0), parent has translation (0,2,0).
    // The child should have world transform = root * parent = translation(1,2,0).
    Scene<Scalar, Index> scene;
    auto mesh_id = scene.add(mesh);

    Eigen::Affine3f root_xf = Eigen::Affine3f::Identity();
    root_xf.translate(Eigen::Vector3f(1.0f, 0.0f, 0.0f));

    Node root;
    root.name = "root";
    root.transform = root_xf;
    auto root_id = scene.add(std::move(root));
    scene.root_nodes.push_back(root_id);

    Eigen::Affine3f parent_xf = Eigen::Affine3f::Identity();
    parent_xf.translate(Eigen::Vector3f(0.0f, 2.0f, 0.0f));

    Node child;
    child.name = "child";
    child.transform = parent_xf;
    child.parent = root_id;

    SceneMeshInstance mi;
    mi.mesh = mesh_id;
    child.meshes.push_back(std::move(mi));

    auto child_id = scene.add(std::move(child));
    scene.nodes[root_id].children.push_back(child_id);

    auto simple = scene_to_simple_scene(scene);

    REQUIRE(simple.get_num_meshes() == 1);
    REQUIRE(simple.get_num_instances(0) == 1);

    // Expected world transform: root_xf * parent_xf = translate(1,2,0).
    Eigen::Affine3f expected_f = root_xf * parent_xf;
    auto expected = expected_f.cast<Scalar>();

    const auto& instance = simple.get_instance(0, 0);
    REQUIRE(instance.transform.matrix().isApprox(expected.matrix(), 1e-6));
}

TEST_CASE("Scene: scene_to_simple_scene multiple meshes and instances", "[scene]")
{
    using Scalar = double;
    using Index = uint32_t;
    using namespace lagrange::scene;

    // Create two meshes.
    lagrange::SurfaceMesh<Scalar, Index> mesh_a;
    mesh_a.add_vertices(3);
    lagrange::vertex_ref(mesh_a).setRandom();
    mesh_a.add_triangle(0, 1, 2);

    lagrange::SurfaceMesh<Scalar, Index> mesh_b;
    mesh_b.add_vertices(4);
    lagrange::vertex_ref(mesh_b).setRandom();
    mesh_b.add_triangle(0, 1, 2);
    mesh_b.add_triangle(0, 2, 3);

    Scene<Scalar, Index> scene;
    auto mesh_a_id = scene.add(mesh_a);
    auto mesh_b_id = scene.add(mesh_b);

    Node root;
    root.name = "root";
    auto root_id = scene.add(std::move(root));
    scene.root_nodes.push_back(root_id);

    // Node referencing mesh_a (identity transform).
    {
        Node n;
        n.parent = root_id;
        SceneMeshInstance mi;
        mi.mesh = mesh_a_id;
        n.meshes.push_back(mi);
        auto nid = scene.add(std::move(n));
        scene.nodes[root_id].children.push_back(nid);
    }

    // Node referencing mesh_b (with a scale transform).
    {
        Eigen::Affine3f xf = Eigen::Affine3f::Identity();
        xf.scale(2.0f);

        Node n;
        n.parent = root_id;
        n.transform = xf;
        SceneMeshInstance mi;
        mi.mesh = mesh_b_id;
        n.meshes.push_back(mi);
        auto nid = scene.add(std::move(n));
        scene.nodes[root_id].children.push_back(nid);
    }

    // Another node also referencing mesh_a (with translation).
    {
        Eigen::Affine3f xf = Eigen::Affine3f::Identity();
        xf.translate(Eigen::Vector3f(5.0f, 0.0f, 0.0f));

        Node n;
        n.parent = root_id;
        n.transform = xf;
        SceneMeshInstance mi;
        mi.mesh = mesh_a_id;
        n.meshes.push_back(mi);
        auto nid = scene.add(std::move(n));
        scene.nodes[root_id].children.push_back(nid);
    }

    auto simple = scene_to_simple_scene(scene);

    REQUIRE(simple.get_num_meshes() == 2);
    // mesh_a has 2 instances, mesh_b has 1 instance.
    REQUIRE(simple.get_num_instances(0) == 2);
    REQUIRE(simple.get_num_instances(1) == 1);

    // Verify mesh vertex counts are preserved.
    REQUIRE(simple.get_mesh(0).get_num_vertices() == 3);
    REQUIRE(simple.get_mesh(1).get_num_vertices() == 4);
}

TEST_CASE("Scene: simple_scene_to_scene basic", "[scene]")
{
    using Scalar = double;
    using Index = uint32_t;
    using namespace lagrange::scene;

    SimpleScene<Scalar, Index> simple;

    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(3);
    lagrange::vertex_ref(mesh).setRandom();
    mesh.add_triangle(0, 1, 2);

    auto mesh_idx = simple.add_mesh(std::move(mesh));

    MeshInstance<Scalar, Index> inst;
    inst.mesh_index = mesh_idx;
    Eigen::Transform<Scalar, 3, Eigen::Affine> xf =
        Eigen::Transform<Scalar, 3, Eigen::Affine>::Identity();
    xf.translate(Eigen::Matrix<Scalar, 3, 1>(1.0, 2.0, 3.0));
    inst.transform = xf;
    simple.add_instance(std::move(inst));

    auto scene = simple_scene_to_scene(simple);

    // One mesh, one root, one child node (for the instance).
    REQUIRE(scene.meshes.size() == 1);
    REQUIRE(scene.root_nodes.size() == 1);
    REQUIRE(scene.nodes.size() == 2); // root + 1 instance node

    // Root node should have one child.
    auto root_id = scene.root_nodes[0];
    REQUIRE(scene.nodes[root_id].children.size() == 1);

    // The child node should reference mesh 0 and carry the transform.
    auto child_id = scene.nodes[root_id].children[0];
    const auto& child = scene.nodes[child_id];
    REQUIRE(child.meshes.size() == 1);
    REQUIRE(child.meshes[0].mesh == 0);
    REQUIRE(child.transform.matrix().isApprox(xf.template cast<float>().matrix(), 1e-5f));
}

TEST_CASE("Scene: simple_scene_to_scene multiple instances", "[scene]")
{
    using Scalar = double;
    using Index = uint32_t;
    using namespace lagrange::scene;

    SimpleScene<Scalar, Index> simple;

    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(3);
    lagrange::vertex_ref(mesh).setRandom();
    mesh.add_triangle(0, 1, 2);
    auto mesh_idx = simple.add_mesh(std::move(mesh));

    // Add 3 instances of the same mesh with different transforms.
    for (int i = 0; i < 3; ++i) {
        MeshInstance<Scalar, Index> inst;
        inst.mesh_index = mesh_idx;
        Eigen::Transform<Scalar, 3, Eigen::Affine> xf =
            Eigen::Transform<Scalar, 3, Eigen::Affine>::Identity();
        xf.translate(
            Eigen::Matrix<Scalar, 3, 1>(
                static_cast<Scalar>(i),
                static_cast<Scalar>(i * 2),
                static_cast<Scalar>(i * 3)));
        inst.transform = xf;
        simple.add_instance(std::move(inst));
    }

    auto scene = simple_scene_to_scene(simple);

    REQUIRE(scene.meshes.size() == 1);
    // root + 3 instance nodes
    REQUIRE(scene.nodes.size() == 4);

    auto root_id = scene.root_nodes[0];
    REQUIRE(scene.nodes[root_id].children.size() == 3);

    // All children should reference mesh 0.
    for (auto child_id : scene.nodes[root_id].children) {
        REQUIRE(scene.nodes[child_id].meshes.size() == 1);
        REQUIRE(scene.nodes[child_id].meshes[0].mesh == 0);
    }
}

TEST_CASE("Scene: roundtrip scene -> simple -> scene", "[scene]")
{
    using Scalar = double;
    using Index = uint32_t;
    using namespace lagrange::scene;

    // Build a scene with a hierarchy.
    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(3);
    auto V = lagrange::vertex_ref(mesh);
    V.row(0) << 0.0, 0.0, 0.0;
    V.row(1) << 1.0, 0.0, 0.0;
    V.row(2) << 0.0, 1.0, 0.0;
    mesh.add_triangle(0, 1, 2);

    Scene<Scalar, Index> scene;
    auto mesh_id = scene.add(mesh);

    Eigen::Affine3f root_xf = Eigen::Affine3f::Identity();
    root_xf.translate(Eigen::Vector3f(1.0f, 0.0f, 0.0f));

    Node root;
    root.name = "root";
    root.transform = root_xf;
    auto root_id = scene.add(std::move(root));
    scene.root_nodes.push_back(root_id);

    Eigen::Affine3f child_xf = Eigen::Affine3f::Identity();
    child_xf.translate(Eigen::Vector3f(0.0f, 2.0f, 0.0f));

    Node child;
    child.name = "child";
    child.transform = child_xf;
    child.parent = root_id;

    SceneMeshInstance mi;
    mi.mesh = mesh_id;
    child.meshes.push_back(std::move(mi));

    auto child_id = scene.add(std::move(child));
    scene.nodes[root_id].children.push_back(child_id);

    // Roundtrip: scene -> simple_scene -> scene2
    auto simple = scene_to_simple_scene(scene);
    auto scene2 = simple_scene_to_scene(simple);

    // The roundtrip scene should have the same number of meshes.
    REQUIRE(scene2.meshes.size() == scene.meshes.size());

    // Mesh data should be preserved.
    REQUIRE(lagrange::vertex_view(scene2.meshes[0]) == lagrange::vertex_view(scene.meshes[0]));
    REQUIRE(lagrange::facet_view(scene2.meshes[0]) == lagrange::facet_view(scene.meshes[0]));

    // The flattened world transform should be preserved.
    // Original world transform = root_xf * child_xf.
    Eigen::Affine3f expected_world = root_xf * child_xf;

    // In scene2, all instance nodes are children of the root.
    // The root is identity, so the child node's local transform IS the world transform.
    REQUIRE(scene2.root_nodes.size() == 1);
    auto root2_id = scene2.root_nodes[0];
    REQUIRE(scene2.nodes[root2_id].children.size() == 1);

    auto child2_id = scene2.nodes[root2_id].children[0];
    const auto& child2 = scene2.nodes[child2_id];
    REQUIRE(child2.transform.matrix().isApprox(expected_world.matrix(), 1e-5f));
}

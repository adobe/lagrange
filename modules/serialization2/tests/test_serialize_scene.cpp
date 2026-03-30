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
#include <lagrange/AttributeValueType.h>
#include <lagrange/primitive/generate_sphere.h>
#include <lagrange/serialization/serialize_scene.h>
#include <lagrange/testing/check_scenes_equal.h>
#include <lagrange/testing/common.h>

#include <lagrange/fs/filesystem.h>

#include <catch2/catch_test_macros.hpp>

#include <Eigen/Geometry>

#include <cmath>

using namespace lagrange::scene;

TEST_CASE("serialization2: empty scene", "[serialization2][scene]")
{
    using Scalar = double;
    using Index = uint32_t;
    Scene<Scalar, Index> scene;
    scene.name = "empty_scene";

    // Need at least one root node
    Node root;
    root.name = "root";
    scene.nodes.push_back(root);
    scene.root_nodes.push_back(0);

    SECTION("uncompressed")
    {
        lagrange::serialization::SerializeOptions opts;
        opts.compress = false;
        auto buf = lagrange::serialization::serialize_scene(scene, opts);
        auto result = lagrange::serialization::deserialize_scene<Scene<Scalar, Index>>(buf);
        lagrange::testing::check_scenes_equal(scene, result);
    }

    SECTION("compressed")
    {
        auto buf = lagrange::serialization::serialize_scene(scene);
        auto result = lagrange::serialization::deserialize_scene<Scene<Scalar, Index>>(buf);
        lagrange::testing::check_scenes_equal(scene, result);
    }
}

TEST_CASE("serialization2: scene with hierarchy", "[serialization2][scene]")
{
    using Scalar = float;
    using Index = uint32_t;
    Scene<Scalar, Index> scene;
    scene.name = "hierarchy_test";

    // Create a mesh
    lagrange::primitive::SphereOptions sphere_opts;
    sphere_opts.num_longitude_sections = 8;
    sphere_opts.num_latitude_sections = 8;
    scene.meshes.push_back(lagrange::primitive::generate_sphere<Scalar, Index>(sphere_opts));

    // Build hierarchy: root -> child1 -> grandchild, root -> child2
    Node root;
    root.name = "root";
    root.transform = Eigen::Affine3f::Identity();
    root.parent = invalid_element;
    root.children.push_back(1);
    root.children.push_back(2);

    Node child1;
    child1.name = "child1";
    child1.transform = Eigen::Affine3f::Identity();
    child1.transform.translate(Eigen::Vector3f(1.f, 0.f, 0.f));
    child1.parent = 0;
    child1.children.push_back(3);
    {
        SceneMeshInstance smi;
        smi.mesh = 0;
        child1.meshes.push_back(std::move(smi));
    }

    Node child2;
    child2.name = "child2";
    child2.transform = Eigen::Affine3f::Identity();
    child2.transform.rotate(Eigen::AngleAxisf(0.5f, Eigen::Vector3f::UnitY()));
    child2.parent = 0;

    Node grandchild;
    grandchild.name = "grandchild";
    grandchild.transform = Eigen::Affine3f::Identity();
    grandchild.transform.scale(Eigen::Vector3f(2.f, 2.f, 2.f));
    grandchild.parent = 1;
    {
        SceneMeshInstance smi;
        smi.mesh = 0;
        grandchild.meshes.push_back(std::move(smi));
    }

    scene.nodes.push_back(std::move(root));
    scene.nodes.push_back(std::move(child1));
    scene.nodes.push_back(std::move(child2));
    scene.nodes.push_back(std::move(grandchild));
    scene.root_nodes.push_back(0);

    auto buf = lagrange::serialization::serialize_scene(scene);
    auto result = lagrange::serialization::deserialize_scene<Scene<Scalar, Index>>(buf);
    lagrange::testing::check_scenes_equal(scene, result);
}

TEST_CASE("serialization2: scene with materials and textures", "[serialization2][scene]")
{
    using Scalar = float;
    using Index = uint32_t;
    Scene<Scalar, Index> scene;

    // Image: 2x2 RGBA
    ImageExperimental img;
    img.name = "test_image";
    img.image.width = 2;
    img.image.height = 2;
    img.image.num_channels = 4;
    img.image.element_type = lagrange::AttributeValueType::e_uint8_t;
    img.image.data.resize(2 * 2 * 4, 128);
    img.uri = lagrange::fs::path("textures/test.png");
    scene.images.push_back(std::move(img));

    // Texture
    Texture tex;
    tex.name = "base_color_tex";
    tex.image = 0;
    tex.mag_filter = Texture::TextureFilter::Linear;
    tex.min_filter = Texture::TextureFilter::LinearMipmapLinear;
    tex.wrap_u = Texture::WrapMode::Clamp;
    tex.wrap_v = Texture::WrapMode::Mirror;
    tex.scale = Eigen::Vector2f(2.f, 3.f);
    tex.offset = Eigen::Vector2f(0.5f, 0.25f);
    tex.rotation = 0.1f;
    scene.textures.push_back(std::move(tex));

    // Material
    MaterialExperimental mat;
    mat.name = "test_material";
    mat.base_color_value = Eigen::Vector4f(0.8f, 0.2f, 0.1f, 1.0f);
    mat.emissive_value = Eigen::Vector3f(0.1f, 0.05f, 0.0f);
    mat.metallic_value = 0.0f;
    mat.roughness_value = 0.7f;
    mat.alpha_mode = MaterialExperimental::AlphaMode::Blend;
    mat.alpha_cutoff = 0.3f;
    mat.normal_scale = 1.5f;
    mat.occlusion_strength = 0.8f;
    mat.double_sided = true;
    mat.base_color_texture.index = 0;
    mat.base_color_texture.texcoord = 0;
    scene.materials.push_back(std::move(mat));

    // Node referencing mesh + material
    lagrange::primitive::SphereOptions sphere_opts;
    sphere_opts.num_longitude_sections = 4;
    sphere_opts.num_latitude_sections = 4;
    scene.meshes.push_back(lagrange::primitive::generate_sphere<Scalar, Index>(sphere_opts));

    Node root;
    root.name = "root";
    SceneMeshInstance smi;
    smi.mesh = 0;
    smi.materials.push_back(0);
    root.meshes.push_back(std::move(smi));
    scene.nodes.push_back(std::move(root));
    scene.root_nodes.push_back(0);

    auto buf = lagrange::serialization::serialize_scene(scene);
    auto result = lagrange::serialization::deserialize_scene<Scene<Scalar, Index>>(buf);
    lagrange::testing::check_scenes_equal(scene, result);
}

TEST_CASE("serialization2: scene with lights", "[serialization2][scene]")
{
    using Scalar = double;
    using Index = uint32_t;
    Scene<Scalar, Index> scene;

    // Point light
    Light point_light;
    point_light.name = "point_light";
    point_light.type = Light::Type::Point;
    point_light.position = Eigen::Vector3f(1.f, 2.f, 3.f);
    point_light.intensity = 100.f;
    point_light.attenuation_constant = 1.f;
    point_light.attenuation_linear = 0.09f;
    point_light.attenuation_quadratic = 0.032f;
    point_light.color_diffuse = Eigen::Vector3f(1.f, 0.9f, 0.8f);
    scene.lights.push_back(std::move(point_light));

    // Spot light
    Light spot_light;
    spot_light.name = "spot_light";
    spot_light.type = Light::Type::Spot;
    spot_light.position = Eigen::Vector3f(0.f, 5.f, 0.f);
    spot_light.direction = Eigen::Vector3f(0.f, -1.f, 0.f);
    spot_light.intensity = 50.f;
    spot_light.angle_inner_cone = 0.3f;
    spot_light.angle_outer_cone = 0.5f;
    spot_light.color_diffuse = Eigen::Vector3f(1.f, 1.f, 1.f);
    scene.lights.push_back(std::move(spot_light));

    // Directional light
    Light dir_light;
    dir_light.name = "dir_light";
    dir_light.type = Light::Type::Directional;
    dir_light.direction = Eigen::Vector3f(0.f, -1.f, -1.f).normalized();
    dir_light.color_diffuse = Eigen::Vector3f(0.5f, 0.5f, 0.5f);
    scene.lights.push_back(std::move(dir_light));

    Node root;
    root.name = "root";
    root.lights.push_back(0);
    root.lights.push_back(1);
    root.lights.push_back(2);
    scene.nodes.push_back(std::move(root));
    scene.root_nodes.push_back(0);

    auto buf = lagrange::serialization::serialize_scene(scene);
    auto result = lagrange::serialization::deserialize_scene<Scene<Scalar, Index>>(buf);
    lagrange::testing::check_scenes_equal(scene, result);
}

TEST_CASE("serialization2: scene with cameras", "[serialization2][scene]")
{
    using Scalar = float;
    using Index = uint32_t;
    Scene<Scalar, Index> scene;

    // Perspective camera with far plane
    Camera persp;
    persp.name = "perspective_cam";
    persp.type = Camera::Type::Perspective;
    persp.position = Eigen::Vector3f(0.f, 1.f, 5.f);
    persp.up = Eigen::Vector3f(0.f, 1.f, 0.f);
    persp.look_at = Eigen::Vector3f(0.f, 0.f, 0.f);
    persp.near_plane = 0.1f;
    persp.far_plane = 1000.f;
    persp.aspect_ratio = 16.f / 9.f;
    persp.horizontal_fov = static_cast<float>(M_PI) / 3.f;
    scene.cameras.push_back(std::move(persp));

    // Perspective camera without far plane
    Camera persp_no_far;
    persp_no_far.name = "infinite_cam";
    persp_no_far.type = Camera::Type::Perspective;
    persp_no_far.near_plane = 0.01f;
    persp_no_far.far_plane = std::nullopt;
    scene.cameras.push_back(std::move(persp_no_far));

    // Orthographic camera
    Camera ortho;
    ortho.name = "ortho_cam";
    ortho.type = Camera::Type::Orthographic;
    ortho.position = Eigen::Vector3f(0.f, 10.f, 0.f);
    ortho.look_at = Eigen::Vector3f(0.f, 0.f, 0.f);
    ortho.near_plane = 0.1f;
    ortho.far_plane = 100.f;
    ortho.orthographic_width = 20.f;
    ortho.aspect_ratio = 1.f;
    scene.cameras.push_back(std::move(ortho));

    Node root;
    root.name = "root";
    root.cameras.push_back(0);
    root.cameras.push_back(1);
    root.cameras.push_back(2);
    scene.nodes.push_back(std::move(root));
    scene.root_nodes.push_back(0);

    auto buf = lagrange::serialization::serialize_scene(scene);
    auto result = lagrange::serialization::deserialize_scene<Scene<Scalar, Index>>(buf);
    lagrange::testing::check_scenes_equal(scene, result);

    // Verify optional far plane handling
    REQUIRE(result.cameras[0].far_plane.has_value());
    REQUIRE(result.cameras[0].far_plane.value() == 1000.f);
    REQUIRE_FALSE(result.cameras[1].far_plane.has_value());
    REQUIRE(result.cameras[2].far_plane.has_value());
}

TEST_CASE("serialization2: scene with extensions", "[serialization2][scene]")
{
    using Scalar = double;
    using Index = uint32_t;
    Scene<Scalar, Index> scene;

    Node root;
    root.name = "root";

    // Test all Value types in extensions
    root.extensions.data["bool_val"] = Value(true);
    root.extensions.data["int_val"] = Value(42);
    root.extensions.data["double_val"] = Value(3.14);
    root.extensions.data["string_val"] = Value(std::string("hello world"));

    Value::Buffer buf_data = {0x01, 0x02, 0x03, 0x04};
    root.extensions.data["buffer_val"] = Value(buf_data);

    // Array
    Value::Array arr;
    arr.push_back(Value(1));
    arr.push_back(Value(std::string("two")));
    arr.push_back(Value(3.0));
    root.extensions.data["array_val"] = Value(std::move(arr));

    // Nested object
    Value::Object obj;
    obj["nested_bool"] = Value(false);
    obj["nested_int"] = Value(-7);
    Value::Array inner_arr;
    inner_arr.push_back(Value(10));
    inner_arr.push_back(Value(20));
    obj["nested_array"] = Value(std::move(inner_arr));
    root.extensions.data["object_val"] = Value(std::move(obj));

    scene.nodes.push_back(std::move(root));
    scene.root_nodes.push_back(0);

    // Also add scene-level extensions
    scene.extensions.data["scene_version"] = Value(std::string("1.0"));

    auto buf = lagrange::serialization::serialize_scene(scene);
    auto result = lagrange::serialization::deserialize_scene<Scene<Scalar, Index>>(buf);

    // Check node extensions
    const auto& ext = result.nodes[0].extensions;
    REQUIRE(ext.data.at("bool_val").get_bool() == true);
    REQUIRE(ext.data.at("int_val").get_int() == 42);
    REQUIRE(ext.data.at("double_val").get_real() == 3.14);
    REQUIRE(ext.data.at("string_val").get_string() == "hello world");

    const auto& result_buf = ext.data.at("buffer_val").get_buffer();
    REQUIRE(result_buf.size() == 4);
    REQUIRE(result_buf[0] == 0x01);
    REQUIRE(result_buf[3] == 0x04);

    const auto& result_arr = ext.data.at("array_val").get_array();
    REQUIRE(result_arr.size() == 3);
    REQUIRE(result_arr[0].get_int() == 1);
    REQUIRE(result_arr[1].get_string() == "two");
    REQUIRE(result_arr[2].get_real() == 3.0);

    const auto& result_obj = ext.data.at("object_val").get_object();
    REQUIRE(result_obj.at("nested_bool").get_bool() == false);
    REQUIRE(result_obj.at("nested_int").get_int() == -7);
    const auto& inner = result_obj.at("nested_array").get_array();
    REQUIRE(inner.size() == 2);
    REQUIRE(inner[0].get_int() == 10);
    REQUIRE(inner[1].get_int() == 20);

    // Check scene-level extensions
    REQUIRE(result.extensions.data.at("scene_version").get_string() == "1.0");
}

TEST_CASE("serialization2: scene with skeletons and animations", "[serialization2][scene]")
{
    using Scalar = float;
    using Index = uint32_t;
    Scene<Scalar, Index> scene;

    lagrange::primitive::SphereOptions sphere_opts;
    sphere_opts.num_longitude_sections = 4;
    sphere_opts.num_latitude_sections = 4;
    scene.meshes.push_back(lagrange::primitive::generate_sphere<Scalar, Index>(sphere_opts));

    Skeleton skel;
    skel.meshes.push_back(0);
    skel.extensions.data["joint_count"] = Value(10);
    scene.skeletons.push_back(std::move(skel));

    Animation anim;
    anim.name = "walk_cycle";
    anim.extensions.data["duration"] = Value(2.5);
    scene.animations.push_back(std::move(anim));

    Node root;
    root.name = "root";
    SceneMeshInstance smi;
    smi.mesh = 0;
    root.meshes.push_back(std::move(smi));
    scene.nodes.push_back(std::move(root));
    scene.root_nodes.push_back(0);

    auto buf = lagrange::serialization::serialize_scene(scene);
    auto result = lagrange::serialization::deserialize_scene<Scene<Scalar, Index>>(buf);
    lagrange::testing::check_scenes_equal(scene, result);

    REQUIRE(result.skeletons[0].meshes.size() == 1);
    REQUIRE(result.skeletons[0].meshes[0] == 0);
    REQUIRE(result.skeletons[0].extensions.data.at("joint_count").get_int() == 10);
    REQUIRE(result.animations[0].name == "walk_cycle");
    REQUIRE(result.animations[0].extensions.data.at("duration").get_real() == 2.5);
}

TEST_CASE("serialization2: scene all type instantiations", "[serialization2][scene]")
{
    SECTION("float, uint32_t")
    {
        Scene<float, uint32_t> scene;
        Node root;
        scene.nodes.push_back(std::move(root));
        scene.root_nodes.push_back(0);
        auto buf = lagrange::serialization::serialize_scene(scene);
        auto result = lagrange::serialization::deserialize_scene<Scene<float, uint32_t>>(buf);
        lagrange::testing::check_scenes_equal(scene, result);
    }

    SECTION("double, uint32_t")
    {
        Scene<double, uint32_t> scene;
        Node root;
        scene.nodes.push_back(std::move(root));
        scene.root_nodes.push_back(0);
        auto buf = lagrange::serialization::serialize_scene(scene);
        auto result = lagrange::serialization::deserialize_scene<Scene<double, uint32_t>>(buf);
        lagrange::testing::check_scenes_equal(scene, result);
    }

    SECTION("float, uint64_t")
    {
        Scene<float, uint64_t> scene;
        Node root;
        scene.nodes.push_back(std::move(root));
        scene.root_nodes.push_back(0);
        auto buf = lagrange::serialization::serialize_scene(scene);
        auto result = lagrange::serialization::deserialize_scene<Scene<float, uint64_t>>(buf);
        lagrange::testing::check_scenes_equal(scene, result);
    }

    SECTION("double, uint64_t")
    {
        Scene<double, uint64_t> scene;
        Node root;
        scene.nodes.push_back(std::move(root));
        scene.root_nodes.push_back(0);
        auto buf = lagrange::serialization::serialize_scene(scene);
        auto result = lagrange::serialization::deserialize_scene<Scene<double, uint64_t>>(buf);
        lagrange::testing::check_scenes_equal(scene, result);
    }
}

TEST_CASE("serialization2: scene type mismatch", "[serialization2][scene]")
{
    using Scalar = float;
    using Index = uint32_t;
    Scene<Scalar, Index> scene;

    lagrange::primitive::SphereOptions sphere_opts;
    sphere_opts.num_longitude_sections = 4;
    sphere_opts.num_latitude_sections = 4;
    scene.meshes.push_back(lagrange::primitive::generate_sphere<Scalar, Index>(sphere_opts));

    Node root;
    root.name = "root";
    scene.nodes.push_back(std::move(root));
    scene.root_nodes.push_back(0);

    auto buf = lagrange::serialization::serialize_scene(scene);

    LA_REQUIRE_THROWS((lagrange::serialization::deserialize_scene<Scene<double, uint32_t>>(buf)));
    LA_REQUIRE_THROWS((lagrange::serialization::deserialize_scene<Scene<float, uint64_t>>(buf)));
}

TEST_CASE("serialization2: scene file round-trip", "[serialization2][scene]")
{
    using Scalar = double;
    using Index = uint32_t;
    Scene<Scalar, Index> scene;
    scene.name = "file_test";

    lagrange::primitive::SphereOptions sphere_opts;
    sphere_opts.num_longitude_sections = 8;
    sphere_opts.num_latitude_sections = 8;
    scene.meshes.push_back(lagrange::primitive::generate_sphere<Scalar, Index>(sphere_opts));

    Node root;
    root.name = "root";
    SceneMeshInstance smi;
    smi.mesh = 0;
    root.meshes.push_back(std::move(smi));
    scene.nodes.push_back(std::move(root));
    scene.root_nodes.push_back(0);

    auto tmp_dir = lagrange::fs::temp_directory_path();

    SECTION("compressed")
    {
        auto path = tmp_dir / "test_scene_compressed.lscene";
        lagrange::serialization::save_scene(path, scene);
        auto result = lagrange::serialization::load_scene<Scene<Scalar, Index>>(path);
        lagrange::testing::check_scenes_equal(scene, result);
        lagrange::fs::remove(path);
    }

    SECTION("uncompressed")
    {
        auto path = tmp_dir / "test_scene_uncompressed.lscene";
        lagrange::serialization::SerializeOptions opts;
        opts.compress = false;
        lagrange::serialization::save_scene(path, scene, opts);
        auto result = lagrange::serialization::load_scene<Scene<Scalar, Index>>(path);
        lagrange::testing::check_scenes_equal(scene, result);
        lagrange::fs::remove(path);
    }
}

TEST_CASE("serialization2: scene user_data gracefully skipped", "[serialization2][scene]")
{
    using Scalar = float;
    using Index = uint32_t;
    Scene<Scalar, Index> scene;

    Node root;
    root.name = "root";
    root.extensions.data["preserved"] = Value(std::string("keep me"));
    root.extensions.user_data["transient"] = std::any(42);
    scene.nodes.push_back(std::move(root));
    scene.root_nodes.push_back(0);

    auto buf = lagrange::serialization::serialize_scene(scene);
    auto result = lagrange::serialization::deserialize_scene<Scene<Scalar, Index>>(buf);

    // data should be preserved
    REQUIRE(result.nodes[0].extensions.data.at("preserved").get_string() == "keep me");

    // user_data should be empty after round-trip (std::any is not serializable)
    REQUIRE(result.nodes[0].extensions.user_data.empty());
}

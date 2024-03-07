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
#include <lagrange/scene/Scene.h>
#include <lagrange/testing/common.h>
#ifdef LAGRANGE_WITH_ASSIMP
    #include <lagrange/io/load_scene_assimp.h>
#endif
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/attribute_names.h>
#include <lagrange/io/load_scene_fbx.h>
#include <lagrange/io/load_scene_gltf.h>
#include <lagrange/io/load_scene_obj.h>
#include <lagrange/io/save_mesh_obj.h>
#include <lagrange/io/save_scene_gltf.h>
#include <lagrange/unify_index_buffer.h>
#include <lagrange/utils/utils.h>
#include <lagrange/views.h>
#include <catch2/catch_approx.hpp>

using namespace lagrange;

template <typename MeshType>
void print_mesh_details(const MeshType& mesh, std::string s = "")
{
    logger().set_level(spdlog::level::debug);
    logger().debug("{}", s);
    logger().debug("mesh has {} facets", mesh.get_num_facets());
    logger().debug("mesh has {} vertices", mesh.get_num_vertices());
    auto f = mesh.get_facet_vertices(0);
    logger().debug("facet 0 verts: {} {} {}", f[0], f[1], f[2]);
    for (int i : f) {
        auto v = mesh.get_position(i);
        logger().debug("v {}: {} {} {}", i, v[0], v[1], v[2]);
    }
    auto uv_id = mesh.get_attribute_id("texcoord_0");
    const Attribute<float>* uv_values = nullptr;
    const Attribute<uint32_t>* uv_indices = nullptr;
    if (mesh.is_attribute_indexed(uv_id)) {
        auto& uvs = mesh.template get_indexed_attribute<float>(uv_id);
        uv_values = &uvs.values();
        uv_indices = &uvs.indices();
    } else {
        auto& uvs = mesh.template get_attribute<float>(uv_id);
        uv_values = &uvs;
        uv_indices = &mesh.get_corner_to_vertex();
    }
    for (int i : f) {
        auto vt = uv_values->get_row(uv_indices->get(i));
        logger().debug("vt {}: {} {}", i, vt[0], vt[1]);
    }
}


TEST_CASE("load_scene_animatedCube", "[io]")
{
    io::LoadOptions opt;
    fs::path filename = testing::get_data_path("open/io/gltf_animated_cube/AnimatedCube.gltf");
    using SceneType = scene::Scene32f;
    SceneType scene;
    SECTION("gltf")
    {
        scene = io::load_scene_gltf<SceneType>(filename, opt);
    }
#ifdef LAGRANGE_WITH_ASSIMP
    SECTION("assimp")
    {
        scene = io::load_scene_assimp<SceneType>(filename, opt);
    }
#endif

    REQUIRE(scene.name.empty()); // this asset has no name
    REQUIRE(!scene.nodes.empty());
    REQUIRE(!scene.meshes.empty());
    REQUIRE(!scene.materials.empty());
    REQUIRE(!scene.textures.empty());
    REQUIRE(scene.lights.empty());
    REQUIRE(scene.cameras.empty());
    REQUIRE(scene.skeletons.empty());
    //REQUIRE(!scene.animations.empty()); // TODO
    auto mesh = scene.meshes.front();
    // assimp is too smart and merges the vertices
    REQUIRE((mesh.get_num_vertices() == 36 || mesh.get_num_vertices() == 24));
    REQUIRE(mesh.get_num_facets() == 12);
    REQUIRE(mesh.has_attribute(AttributeName::normal));
    REQUIRE(mesh.has_attribute(std::string(AttributeName::texcoord) + "_0"));
}

TEST_CASE("load_scene_avocado", "[io]")
{
    io::LoadOptions opt;
    using SceneType = scene::Scene32f;
    using MeshType = SceneType::MeshType;

    SceneType scene;
    std::string uv_attr_name = std::string(AttributeName::texcoord) + "_0";
    fs::path avocado_path = testing::get_data_path("open/io/avocado");
    opt.search_path = avocado_path;
    bool from_obj = false;
    SECTION("gltf")
    {
        scene = io::load_scene_gltf<SceneType>(avocado_path / "Avocado.gltf", opt);
        // for manual debugging:
        // io::save_scene_gltf("avocado.gltf", scene);
        // print_mesh_details(scene.meshes.front(), "gltf");
    }
#ifdef LAGRANGE_WITH_ASSIMP
    SECTION("assimp")
    {
        scene = io::load_scene_assimp<SceneType>(avocado_path / "Avocado.gltf", opt);
        // for manual debugging:
        // io::save_scene_gltf("avocado.gltf", scene);
        // print_mesh_details(scene.meshes.front(), "assimp");
    }
#endif
    SECTION("fbx")
    {
        scene = io::load_scene_fbx<SceneType>(avocado_path / "avocado.fbx", opt);
        // uv_attr_name = "UVMap";
        scene.meshes.front().rename_attribute("UVMap", uv_attr_name);
        // for manual debugging. note that we need the unified version for saving, but not for the
        // tests below.
        // auto mesh1 = scene.meshes.front();
        // auto mesh2 = unify_index_buffer(mesh1);
        // scene.meshes[0] = mesh2;
        // io::save_scene_gltf("avocado.gltf", scene);
        // scene.meshes[0] = mesh1;
        // print_mesh_details(scene.meshes.front(), "fbx");
    }
    SECTION("obj")
    {
        scene = io::load_scene_obj<SceneType>(avocado_path / "avocado.obj", opt);
        from_obj = true;
        scene.meshes.front().rename_attribute("texcoord", uv_attr_name);
        // io::save_scene_gltf("avocado.gltf", scene);
        // print_mesh_details(scene.meshes.front(), "obj");
    }
    REQUIRE(!scene.nodes.empty());
    REQUIRE(!scene.meshes.empty());
    REQUIRE(!scene.materials.empty());
    REQUIRE(scene.textures.size() >= 2); // 2 or 3
    REQUIRE(scene.images.size() >= 2); // 2 or 3
    REQUIRE(scene.images.front().height > 0);
    REQUIRE(scene.images.front().width > 0);
    REQUIRE(scene.lights.empty());
    REQUIRE(scene.cameras.empty());
    REQUIRE(scene.skeletons.empty());
    REQUIRE(scene.animations.empty());
    MeshType mesh = scene.meshes.front();
    REQUIRE(mesh.get_num_vertices() == 406);
    REQUIRE(mesh.get_num_facets() == 682);
    REQUIRE(mesh.has_attribute(uv_attr_name));
    REQUIRE(mesh.has_attribute(AttributeName::normal));

    auto f0 = mesh.get_facet_vertices(0);
    REQUIRE(f0.size() == 3);
    auto v0 = mesh.get_position(f0[0]);
    auto v1 = mesh.get_position(f0[1]);
    auto v2 = mesh.get_position(f0[2]);
    const float p = from_obj ? 1e-4f : 1e-6f;
    REQUIRE(Eigen::Vector3f(v0[0], v0[1], v0[2])
                .isApprox(Eigen::Vector3f(-0.0013003338f, 0.014820849f, -0.0075045973f), p));
    REQUIRE(Eigen::Vector3f(v1[0], v1[1], v1[2])
                .isApprox(Eigen::Vector3f(-0.0036110256f, 0.015894055f, -0.0081206625f), p));
    REQUIRE(Eigen::Vector3f(v2[0], v2[1], v2[2])
                .isApprox(Eigen::Vector3f(-0.0027212794f, 0.016771588f, -0.009253962f), p));

    auto uv_id = mesh.get_attribute_id(uv_attr_name);
    const Attribute<float>* uv_values = nullptr;
    const Attribute<uint32_t>* uv_indices = nullptr;
    if (mesh.is_attribute_indexed(uv_id)) {
        auto& uvs = mesh.get_indexed_attribute<float>(uv_id);
        uv_values = &uvs.values();
        uv_indices = &uvs.indices();
    } else {
        auto& uvs = mesh.get_attribute<float>(uv_id);
        uv_values = &uvs;
        uv_indices = &mesh.get_corner_to_vertex();
    }
    std::vector<Eigen::Vector2f> vt_truth = {
        Eigen::Vector2f(0.86037403, 0.66977674),
        Eigen::Vector2f(0.88697016, 0.687139),
        Eigen::Vector2f(0.87410265, 0.7009108)};
    for (int i = 0; i < 3; ++i) {
        auto vt = uv_values->get_row(uv_indices->get(f0[i]));
        REQUIRE(Eigen::Vector2f(vt[0], vt[1]).isApprox(vt_truth[f0[i]], p));
    }
}

TEST_CASE("load_scene_cameras", "[io]")
{
    using SceneType = scene::Scene32f;
    SceneType scene;
    io::LoadOptions opt;
    fs::path cameras_file = testing::get_data_path("open/io/two_cameras.gltf");
    SECTION("gltf")
    {
        scene = io::load_scene_gltf<SceneType>(cameras_file, opt);
    }
#ifdef LAGRANGE_WITH_ASSIMP
    SECTION("assimp")
    {
        scene = io::load_scene_assimp<SceneType>(cameras_file, opt);
    }
#endif
    REQUIRE(scene.cameras.size() == 2);
    auto& ortho = scene.cameras[0];
    auto& persp = scene.cameras[1];
    REQUIRE(ortho.type == scene::Camera::Type::Orthographic);
    REQUIRE(ortho.orthographic_width == 6.f);
    REQUIRE(ortho.aspect_ratio == 1.f);
    REQUIRE(ortho.near_plane == 0.1f);
    REQUIRE(ortho.far_plane == 100.f);
    REQUIRE(ortho.horizontal_fov == 0.f);

    REQUIRE(persp.type == scene::Camera::Type::Perspective);
    REQUIRE(persp.near_plane == 0.1f);
    REQUIRE(persp.far_plane == 1000.f);
    REQUIRE(persp.horizontal_fov == lagrange::to_radians(60.f));
    REQUIRE(persp.aspect_ratio == 1920.f / 1080.f);
}


TEST_CASE("load_save_scene_animatedCube", "[io]")
{
    using SceneType = scene::Scene32f;
    SceneType scene;
    io::LoadOptions load_opt;

    scene = io::load_scene_gltf<SceneType>(
        testing::get_data_path("open/io/gltf_animated_cube/AnimatedCube.gltf"),
        load_opt);

    io::SaveOptions save_opt;
    io::save_scene_gltf("animatedCube.gltf", scene, save_opt);
}

TEST_CASE("load_save_scene_fbx", "[io]")
{
    io::LoadOptions load_opt;
    io::SaveOptions save_opt;
    auto avocado_path = testing::get_data_path("open/io/avocado/avocado.fbx");
    load_opt.search_path = testing::get_data_path("open/io/avocado/");
    auto scene32f = io::load_scene_fbx<scene::Scene32f>(avocado_path, load_opt);
    io::save_scene_gltf("avocado32f.gltf", scene32f, save_opt);

    auto scene64d = io::load_scene_fbx<scene::Scene64d>(avocado_path, load_opt);
    io::save_scene_gltf("avocado64d.gltf", scene64d, save_opt);
}

TEST_CASE("load_save_scene_obj", "[io]")
{
    io::LoadOptions load_opt;
    load_opt.search_path = testing::get_data_path("open/io/avocado/");
    auto avocado_path = testing::get_data_path("open/io/avocado/avocado.obj");
    auto scene32f = io::load_scene_obj<scene::Scene32f>(avocado_path, load_opt);

    io::save_scene_gltf("avocado_from_obj.gltf", scene32f);
    auto mesh = scene32f.meshes.front();
    io::save_mesh_obj("avocado_from_obj.obj", mesh);
}

TEST_CASE("load_gltf_gsplat", "[io]" LA_CORP_FLAG) {
    auto scene = io::load_scene_gltf<scene::Scene32f>(
        testing::get_data_path("corp/io/neural_assets/High_Heel.gltf"));
    REQUIRE(!scene.nodes[0].extensions.data.empty());
    const auto& value = scene.nodes[0].extensions.data["ADOBE_gsplat_asset"];
    REQUIRE(value.size() == 125);
}

TEST_CASE("load_gltf_nerf", "[io]" LA_CORP_FLAG)
{
    auto scene = io::load_scene_gltf<scene::Scene32f>(
        testing::get_data_path("corp/io/neural_assets/Toy_Car.gltf"));
    REQUIRE(!scene.nodes[0].extensions.data.empty());
    const auto& value = scene.nodes[0].extensions.data["ADOBE_nerf_asset"];
    REQUIRE(value.size() == 49);
}

TEST_CASE("scene_extension_user", "[scene]")
{
    struct MyValue
    {
        int splat_count;
    };
    class MyConverter : public lagrange::scene::UserDataConverter
    {
        bool is_supported(const std::string& key) const override
        {
            return key == "ADOBE_gsplat_asset";
        }
        bool can_write(const std::string&) const override { return false; }
        lagrange::scene::Value write(const std::any&) const override
        {
            return lagrange::scene::Value();
        }
        std::any read(const lagrange::scene::Value& value) const override
        {
            MyValue val;
            val.splat_count = value["splat_count"].get_int();
            return val;
        }
    };

    MyConverter converter;
    io::LoadOptions load_opt;
    load_opt.extension_converters = {&converter};
    auto scene = io::load_scene_gltf<scene::Scene32f>(
        testing::get_data_path("corp/io/neural_assets/High_Heel.gltf"), load_opt);

    REQUIRE(scene.nodes[0].extensions.data.size() == 0);
    REQUIRE(scene.nodes[0].extensions.user_data.size() == 1);
    MyValue val = std::any_cast<MyValue>(scene.nodes[0].extensions.user_data["ADOBE_gsplat_asset"]);
    REQUIRE(val.splat_count == 104783);
}

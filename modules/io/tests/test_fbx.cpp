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
#include <lagrange/io/load_mesh_fbx.h>
#include <lagrange/io/load_mesh_ply.h>
#include <lagrange/io/load_scene_fbx.h>
#include <lagrange/io/save_scene.h>
#include <lagrange/scene/internal/scene_string_utils.h>
#include <lagrange/scene/scene_convert.h>
#include <lagrange/views.h>

#include <lagrange/testing/common.h>
#include <lagrange/testing/equivalence_check.h>

TEST_CASE("load_fbx", "[io][fbx]" LA_CORP_FLAG)
{
    lagrange::io::LoadOptions options;
    options.quiet = true;
    auto mesh = lagrange::io::load_mesh_fbx<lagrange::SurfaceMesh32d>(
        lagrange::testing::get_data_path("corp/io/cgt_c_table_coffee_table_003.fbx"),
        options);
    auto expected = lagrange::io::load_mesh_ply<lagrange::SurfaceMesh32d>(
        lagrange::testing::get_data_path("corp/io/cgt_c_table_coffee_table_003.ply"),
        options);
    REQUIRE(vertex_view(mesh) == vertex_view(expected));
    REQUIRE(facet_view(mesh) == facet_view(expected));
}

TEST_CASE("load_fbx_and_save", "[io][fbx]" LA_CORP_FLAG)
{
    lagrange::io::LoadOptions load_options;
    lagrange::io::SaveOptions save_options;
    load_options.quiet = true;
    save_options.quiet = true;
    auto scene = lagrange::io::load_scene_fbx<lagrange::scene::Scene32d>(
        lagrange::testing::get_data_path("corp/io/buffet_gray_001.fbx"),
        load_options);
    lagrange::fs::path output_glb =
        lagrange::testing::get_test_output_path("test_io/buffet_gray_001.glb");
    lagrange::fs::path output_obj =
        lagrange::testing::get_test_output_path("test_io/buffet_gray_001.obj");
    REQUIRE_NOTHROW(lagrange::io::save_scene(output_glb, scene, save_options));
    REQUIRE_NOTHROW(lagrange::io::save_scene(output_obj, scene, save_options));
}

TEST_CASE("load_fbx with duplicate attr", "[io][fbx]")
{
    lagrange::io::LoadOptions options;
    options.quiet = true;
    REQUIRE_NOTHROW(
        lagrange::io::load_mesh_fbx<lagrange::SurfaceMesh32d>(
            lagrange::testing::get_data_path("open/io/Walking.fbx"),
            options));
}

TEST_CASE("load_fbx with geometric transform", "[io][fbx]" LA_CORP_FLAG)
{
    lagrange::io::LoadOptions options;
    options.quiet = true;
    auto mesh = lagrange::io::load_mesh_fbx<lagrange::SurfaceMesh32d>(
        lagrange::testing::get_data_path("corp/io/cgt_fmcg_stain_remove_whiten_049.fbx"),
        options);
    auto scene = lagrange::io::load_scene_fbx<lagrange::scene::Scene32d>(
        lagrange::testing::get_data_path("corp/io/cgt_fmcg_stain_remove_whiten_049.fbx"),
        options);
    auto flat = lagrange::scene::scene_to_mesh(scene);
    auto expected = lagrange::io::load_mesh_ply<lagrange::SurfaceMesh32d>(
        lagrange::testing::get_data_path("corp/io/cgt_fmcg_stain_remove_whiten_049.ply"),
        options);
    lagrange::logger().debug("{}", lagrange::scene::internal::to_string(scene));
    mesh = lagrange::SurfaceMesh32d::stripped_move(std::move(mesh));
    flat = lagrange::SurfaceMesh32d::stripped_move(std::move(flat));
    expected = lagrange::SurfaceMesh32d::stripped_move(std::move(expected));
    lagrange::testing::ensure_approx_equivalent_mesh(mesh, expected);
    lagrange::testing::ensure_approx_equivalent_mesh(flat, expected);
}

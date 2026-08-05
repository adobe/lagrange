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
#include <lagrange/attribute_names.h>
#include <lagrange/io/load_mesh_fbx.h>
#include <lagrange/io/load_mesh_ply.h>
#include <lagrange/io/load_scene_fbx.h>
#include <lagrange/io/save_scene.h>
#include <lagrange/scene/internal/scene_string_utils.h>
#include <lagrange/scene/scene_convert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/views.h>

#include <lagrange/testing/common.h>
#include <lagrange/testing/equivalence_check.h>

#include <algorithm>

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

    REQUIRE_THROWS(lagrange::io::load_mesh_fbx<lagrange::SurfaceMesh32d>("file_not_exist.fbx"));
    REQUIRE_THROWS(lagrange::io::load_scene_fbx<lagrange::scene::Scene32d>("file_not_exist.fbx"));
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

TEST_CASE("load_fbx face_material preserved", "[io][fbx][scene]" LA_CORP_FLAG)
{
    // Verify that ufbx's per-face material assignment is preserved as a material_id
    // facet attribute on the loaded mesh, not silently dropped.
    lagrange::io::LoadOptions options;
    options.quiet = true;
    auto scene = lagrange::io::load_scene_fbx<lagrange::scene::Scene32d>(
        lagrange::testing::get_data_path(
            "corp/io/human_alloy_failed_00036_Esmee003_Basics_001.fbx"),
        options);

    // At least one raw mesh must carry the per-facet material_id attribute written
    // by the loader from ufbx::face_material.
    bool found_attr = false;
    for (const auto& mesh : scene.meshes) {
        if (mesh.has_attribute(lagrange::AttributeName::material_id)) {
            found_attr = true;
            break;
        }
    }
    REQUIRE(found_attr);

    // After scene_to_meshes_and_materials, at least one output mesh must have more than
    // one distinct per-instance material index — confirming the per-face data was loaded
    // instead of a flat materials.front() assignment.
    auto result = lagrange::scene::scene_to_meshes_and_materials(scene);
    bool found_multi = false;
    for (size_t mi = 0; mi < result.meshes.size(); ++mi) {
        const auto& mesh = result.meshes[mi];
        if (!mesh.has_attribute(lagrange::AttributeName::material_id)) continue;
        const auto& attr = mesh.template get_attribute<lagrange::SurfaceMesh32d::Index>(
            lagrange::AttributeName::material_id);
        const auto vals = attr.get_all();
        if (vals.size() < 2) continue;
        // Count how many per-instance material indices this mesh references.
        // invalid<Index>() means "no material".
        // Also verify all valid IDs are in-range for this mesh's material list.
        const auto num_mats =
            static_cast<lagrange::SurfaceMesh32d::Index>(result.material_ids[mi].size());
        const auto inv = lagrange::invalid<lagrange::SurfaceMesh32d::Index>();
        bool all_in_range = true;
        lagrange::SurfaceMesh32d::Index first_valid = inv;
        bool has_second = false;
        for (auto id : vals) {
            if (id == inv) continue;
            if (id >= num_mats) {
                all_in_range = false;
                break;
            }
            if (first_valid == inv) {
                first_valid = id;
            } else if (id != first_valid) {
                has_second = true;
            }
        }
        if (all_in_range && has_second) {
            found_multi = true;
            break;
        }
    }
    REQUIRE(found_multi);
}

TEST_CASE("load_fbx embedded textures", "[io][fbx]")
{
    lagrange::io::LoadOptions options;
    options.load_images = true;
    options.quiet = true;

    auto scene = lagrange::io::load_scene_fbx<lagrange::scene::Scene32d>(
        lagrange::testing::get_data_path("open/io/Walking.fbx"),
        options);

    // Walking.fbx has embedded textures — at least one image must have decoded pixel data.
    bool any_loaded = std::any_of(scene.images.begin(), scene.images.end(), [](const auto& img) {
        return img.image.width > 0 && img.image.height > 0 && !img.image.data.empty();
    });
    REQUIRE(any_loaded);
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

TEST_CASE(
    "scene_to_meshes_and_materials mesh/material count match",
    "[io][fbx][scene]" LA_CORP_FLAG)
{
    lagrange::io::LoadOptions options;
    options.quiet = true;
    auto scene = lagrange::io::load_scene_fbx<lagrange::scene::Scene32d>(
        lagrange::testing::get_data_path(
            "corp/io/human_alloy_failed_00036_Esmee003_Basics_001.fbx"),
        options);

    auto result = lagrange::scene::scene_to_meshes_and_materials(scene);

    // meshes[i] and material_ids[i] must be in 1-to-1 correspondence.
    REQUIRE(result.meshes.size() == result.material_ids.size());

    for (size_t i = 0; i < result.meshes.size(); ++i) {
        const auto& mesh = result.meshes[i];
        const auto& mat_ids = result.material_ids[i];

        // Every material index in the per-mesh list must be a valid scene material index.
        for (auto mat_id : mat_ids) {
            REQUIRE(mat_id < scene.materials.size());
        }

        // scene_to_meshes_and_materials must always produce a material_id facet attribute.
        REQUIRE(mesh.has_attribute(lagrange::AttributeName::material_id));
        const auto& attr = mesh.template get_attribute<lagrange::SurfaceMesh32d::Index>(
            lagrange::AttributeName::material_id);
        REQUIRE(attr.get_num_elements() == mesh.get_num_facets());

        for (auto facet_mat_id : attr.get_all()) {
            const bool is_invalid =
                facet_mat_id == lagrange::invalid<lagrange::SurfaceMesh32d::Index>();
            // Per-facet value is either a valid scene material index or the no-material sentinel.
            REQUIRE((is_invalid || facet_mat_id < scene.materials.size()));
            if (!is_invalid) {
                // It must also appear in the per-mesh material_ids list.
                bool found = false;
                for (auto id : mat_ids) {
                    if (id == facet_mat_id) {
                        found = true;
                        break;
                    }
                }
                REQUIRE(found);
            }
        }
    }
}

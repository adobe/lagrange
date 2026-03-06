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

#include "image_helpers.h"

#include <lagrange/texproc/extract_mesh_with_alpha_mask.h>

#include <lagrange/image/Array3D.h>
#include <lagrange/io/load_scene.h>
#include <lagrange/scene/Scene.h>
#include <lagrange/scene/scene_utils.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>

#include <spdlog/spdlog.h>

void run_mesh_with_alpha_mask(const lagrange::fs::path& path)
{
    auto scene_options = lagrange::io::LoadOptions();
    scene_options.stitch_vertices = true;
    auto scene = lagrange::io::load_scene<lagrange::scene::Scene32f>(path, scene_options);

    // retrieve single instance
    REQUIRE(scene.nodes.size() == 1);
    const auto& node = scene.nodes.front();
    REQUIRE(node.meshes.size() == 1);
    const auto& instance = node.meshes.front();

    // retrieve material
    REQUIRE(instance.materials.size() == 1);
    const auto& material = scene.materials.at(instance.materials.front());
    REQUIRE(material.alpha_mode == lagrange::scene::MaterialExperimental::AlphaMode::Blend);
    REQUIRE(material.alpha_cutoff >= 0.0);
    REQUIRE(material.alpha_cutoff <= 1.0);
    const auto image =
        test::scene_image_to_image_array(scene.images.at(material.base_color_texture.index).image);
    REQUIRE(image.extent(2) == 4);

    // retrieve mesh
    auto mesh = scene.meshes.at(instance.mesh);
    const auto texcoord_id =
        mesh.get_attribute_id(fmt::format("texcoord_{}", material.base_color_texture.texcoord));
    REQUIRE(texcoord_id != lagrange::invalid_attribute_id());
    REQUIRE(mesh.is_attribute_indexed(texcoord_id));
    REQUIRE(mesh.is_triangle_mesh());

    SECTION("check transform")
    {
        const auto node_transform = lagrange::scene::utils::compute_global_node_transform(scene, 0);
        REQUIRE(node_transform.isApprox(Eigen::Affine3f::Identity()));
    }

    SECTION("extract mesh")
    {
        // extract mesh from mesh and alpha mask
        lagrange::texproc::ExtractMeshWithAlphaMaskOptions extract_options;
        extract_options.texcoord_id = texcoord_id;
        extract_options.alpha_threshold = material.alpha_cutoff;
        const auto mesh_ = lagrange::texproc::extract_mesh_with_alpha_mask(
            mesh,
            image.to_mdspan(),
            extract_options);
        REQUIRE(mesh_.get_num_facets() > 0);
    }
}

TEST_CASE("extract cube with transparent numbers", "[texproc][alpha_mask]")
{
    const auto path = lagrange::testing::get_data_dir() / "corp/texproc/alpha_cube_numbers.glb";
    run_mesh_with_alpha_mask(path);
}

TEST_CASE("extract cube with transparent letters", "[texproc][alpha_mask]")
{
    const auto path = lagrange::testing::get_data_dir() / "corp/texproc/alpha_cube_letters.glb";
    run_mesh_with_alpha_mask(path);
}

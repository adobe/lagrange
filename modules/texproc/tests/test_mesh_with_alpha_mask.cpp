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

#include "../shared/shared_utils.h"
#include "image_helpers.h"

#include <lagrange/texproc/extract_mesh_with_alpha_mask.h>

#include <lagrange/image/Array3D.h>
#include <lagrange/io/load_scene.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/io/save_scene_gltf.h>
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

TEST_CASE("ambiguous marching squares saddle case", "[texproc][alpha_mask]")
{
    using Scalar = float;
    using Index = uint32_t;

    // Create a single triangle mesh covering most of [0,1]².
    lagrange::SurfaceMesh<Scalar, Index> mesh(3);
    mesh.add_vertex({0.0f, 0.0f, 0.0f});
    mesh.add_vertex({1.0f, 0.0f, 0.0f});
    mesh.add_vertex({0.5f, 1.0f, 0.0f});
    mesh.add_triangle(0, 1, 2);

    // Add indexed UV attribute matching the vertex XY positions.
    const Scalar uv_data[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f};
    const Index uv_indices[] = {0, 1, 2};
    auto texcoord_id = mesh.create_attribute<Scalar>(
        "texcoord_0",
        lagrange::AttributeElement::Indexed,
        2,
        lagrange::AttributeUsage::UV,
        {uv_data, 6},
        {uv_indices, 3});

    // Create 2x2 RGBA image with saddle pattern.
    // sample_alpha(uv) reads image(ij(0), height-1-ij(1), 3).
    //   BL texel (0,0) -> image(0,1,3)  opaque
    //   BR texel (1,0) -> image(1,1,3)  transparent
    //   TL texel (0,1) -> image(0,0,3)  transparent
    //   TR texel (1,1) -> image(1,0,3)  opaque
    auto image = lagrange::image::experimental::create_image<float>(2, 2, 4);
    for (size_t x = 0; x < 2; ++x) {
        for (size_t y = 0; y < 2; ++y) {
            for (size_t c = 0; c < 4; ++c) {
                image(x, y, c) = 0.0f;
            }
        }
    }
    image(0, 1, 3) = 1.0f; // BL opaque
    image(1, 0, 3) = 1.0f; // TR opaque

    // Save the saddle input as a glTF scene with embedded texture (enable for debugging)
    if (0) {
        lagrange::scene::internal::SingleMeshToSceneOptions scene_options;
        scene_options.alpha_mode = lagrange::scene::MaterialExperimental::AlphaMode::Blend;
        scene_options.alpha_cutoff = 0.5f;
        auto scene = lagrange::scene::internal::single_mesh_to_scene(
            lagrange::SurfaceMesh<Scalar, Index>(mesh),
            image.to_mdspan(),
            scene_options);
        lagrange::io::save_scene_gltf("saddle_input.glb", scene);
    }

    SECTION("disconnected saddle produces separate polygons")
    {
        // center_alpha = (1+0+0+1)/4 = 0.5, threshold = 0.5 -> disconnected (two polygons)
        lagrange::texproc::ExtractMeshWithAlphaMaskOptions options;
        options.texcoord_id = texcoord_id;
        options.alpha_threshold = 0.5f;
        const auto result =
            lagrange::texproc::extract_mesh_with_alpha_mask(mesh, image.to_mdspan(), options);
        // lagrange::io::save_mesh("saddle_disconnected.obj", result);

        // Should produce at least 2 facets (one per opaque corner).
        REQUIRE(result.get_num_facets() >= 2);

        // All output facets must have positive signed area (no degenerate/inverted triangles).
        const auto verts = lagrange::vertex_view(result);
        for (Index f = 0; f < result.get_num_facets(); ++f) {
            const auto facet = result.get_facet_vertices(f);
            if (facet.size() >= 3) {
                Eigen::Vector3f v0 = verts.row(facet[0]).transpose();
                Eigen::Vector3f v1 = verts.row(facet[1]).transpose();
                Eigen::Vector3f v2 = verts.row(facet[2]).transpose();
                float area = (v1 - v0).cross(v2 - v0).z() * 0.5f;
                REQUIRE(area > 1e-6f);
            }
        }
    }

    SECTION("connected saddle produces merged polygon")
    {
        // center_alpha = 0.5 > 0.4 -> connected (single merged polygon)
        lagrange::texproc::ExtractMeshWithAlphaMaskOptions options;
        options.texcoord_id = texcoord_id;
        options.alpha_threshold = 0.4f;
        const auto result =
            lagrange::texproc::extract_mesh_with_alpha_mask(mesh, image.to_mdspan(), options);
        // lagrange::io::save_mesh("saddle_connected.obj", result);
        REQUIRE(result.get_num_facets() >= 1);
    }
}

TEST_CASE("extract cube with transparent numbers", "[texproc][alpha_mask]" LA_CORP_FLAG)
{
    const auto path = lagrange::testing::get_data_dir() / "corp/texproc/alpha_cube_numbers.glb";
    run_mesh_with_alpha_mask(path);
}

TEST_CASE("extract cube with transparent letters", "[texproc][alpha_mask]" LA_CORP_FLAG)
{
    const auto path = lagrange::testing::get_data_dir() / "corp/texproc/alpha_cube_letters.glb";
    run_mesh_with_alpha_mask(path);
}

/*
 * Copyright 2025 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
////////////////////////////////////////////////////////////////////////////////

#include "../examples/io_helpers.h"
#include "../shared/shared_utils.h"

#include <lagrange/image/Array3D.h>
#include <lagrange/image/View3D.h>
#include <lagrange/io/load_scene.h>
#include <lagrange/texproc/TextureRasterizer.h>
#include <lagrange/texproc/texture_compositing.h>
#include <lagrange/utils/range.h>

#include <lagrange/testing/common.h>

#include <catch2/catch_test_macros.hpp>

#include <numeric>

namespace {

void test_pipeline(
    const lagrange::SurfaceMesh32f& mesh,
    const std::vector<lagrange::texproc::CameraOptions>& cameras,
    const std::vector<Array3Df>& views,
    size_t width,
    size_t height)
{
    auto rasterizer_options = lagrange::texproc::TextureRasterizerOptions();
    rasterizer_options.width = width;
    rasterizer_options.height = height;
    const auto rasterizer = lagrange::texproc::TextureRasterizer(mesh, rasterizer_options);

    std::vector<std::pair<Array3Df, Array3Df>> colors_and_weights;
    for (const auto kk : lagrange::range(cameras.size())) {
        const auto& camera = cameras[kk];
        const auto& view = views[kk];
        colors_and_weights.emplace_back(
            rasterizer.weighted_texture_from_render(view.to_mdspan(), camera));
    }

    REQUIRE(cameras.size() == colors_and_weights.size());

    using WeightedTexture = lagrange::texproc::ConstWeightedTextureView<float>;
    std::vector<WeightedTexture> weighted_textures;
    for (const auto& [color, weight] : colors_and_weights) {
        weighted_textures.emplace_back(
            WeightedTexture{
                color.to_mdspan(),
                weight.to_mdspan(),
            });
    }

    const auto compositing_options = lagrange::texproc::CompositingOptions();
    auto final_color =
        lagrange::texproc::texture_compositing(mesh, weighted_textures, compositing_options);

    REQUIRE(final_color.extent(0) == width);
    REQUIRE(final_color.extent(1) == height);
    REQUIRE(final_color.extent(2) == 3);
}

} // namespace

TEST_CASE("Pumpkin pipeline", "[texproc]" LA_SLOW_DEBUG_FLAG LA_CORP_FLAG)
{
    auto scene_options = lagrange::io::LoadOptions();
    scene_options.stitch_vertices = true;
    const auto scene = lagrange::io::load_scene<lagrange::scene::Scene32f>(
        lagrange::testing::get_data_path("corp/texproc/prepared/pumpkin.glb"),
        scene_options);

    const auto& [mesh, _] = lagrange::texproc::single_mesh_from_scene(scene);
    const auto cameras = lagrange::texproc::cameras_from_scene(scene);
    REQUIRE(cameras.size() == 16);

    std::vector<Array3Df> views;
    for (const auto kk : lagrange::range(cameras.size())) {
        views.emplace_back(load_image(
            lagrange::testing::get_data_path(
                fmt::format("corp/texproc/prepared/view_{:02d}.png", kk))));
    }
    REQUIRE(cameras.size() == views.size());

    SECTION("1024x1024")
    {
        test_pipeline(mesh, cameras, views, 1024, 1024);
    }
    SECTION("512x512")
    {
        test_pipeline(mesh, cameras, views, 512, 512);
    }
    SECTION("512x1024")
    {
        test_pipeline(mesh, cameras, views, 512, 1024);
    }
}

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
#include "../shared/shared_utils.h"
#include "io_helpers.h"

#include <lagrange/find_matching_attributes.h>
#include <lagrange/image/split_grid.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/load_scene.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/io/save_scene.h>
#include <lagrange/scene/scene_convert.h>
#include <lagrange/scene/scene_utils.h>
#include <lagrange/texproc/TextureRasterizer.h>
#include <lagrange/utils/fmt/format.h>

#include <tbb/parallel_for.h>

#include <CLI/CLI.hpp>

namespace fs = lagrange::fs;

fs::path make_output_path(const fs::path& base_path, size_t index)
{
    return base_path.parent_path() / lagrange::format(
                                         "{}_{:02d}{}",
                                         base_path.stem().string(),
                                         index,
                                         base_path.extension().string());
}

/* Example usage:

    ./examples/Release/texture_rasterization \
        --scene-in ../data/corp/texproc/prepared/pumpkin.glb \
        --renders-in ../data/corp/texproc/prepared/view_*.png \
        --width 1024 --height 1024 --base-confidence 0 \
        --meshes-out pumpkin.glb

*/
int main(int argc, char** argv)
{
    struct
    {
        fs::path input_scene;
        fs::path input_texture;
        std::vector<fs::path> input_renders;
        fs::path input_render_grid;
        fs::path output_textures = "output_textures.exr";
        fs::path output_weights = "output_weights.exr";
        fs::path output_meshes;
        std::optional<size_t> width;
        std::optional<size_t> height;
        float low_confidence_ratio = 0.75;
        std::optional<float> base_confidence;
        int log_level = 2;
    } args;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option(
           "--scene-in",
           args.input_scene,
           "Input scene containing a single mesh with UVs (optionally associated with a base "
           "texture), and cameras.")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("--texture-in", args.input_texture, "Override input base texture.")
        ->check(CLI::ExistingFile);
    app.add_option("--renders-in", args.input_renders, "Input rendered images.")
        ->check(CLI::ExistingFile);
    app.add_option(
           "--render-grid-in",
           args.input_render_grid,
           "Input single grid image containing all renders. "
           "The grid is split based on the number of cameras in the scene.")
        ->check(CLI::ExistingFile);
    app.add_option(
        "--textures-out",
        args.output_textures,
        "Output base name for color texture images.");
    app.add_option(
        "--weights-out",
        args.output_weights,
        "Output base name for confidence weight texture images.");
    app.add_option(
        "--meshes-out",
        args.output_meshes,
        "Output base name for textured mesh files (.obj or .glb).");
    app.add_option(
        "--base-confidence",
        args.base_confidence,
        "Uniform confidence assigned to the base texture.");
    app.add_option(
        "--width",
        args.width,
        "Rasterization texture width. Defaults to 1024 if no base texture is provided.");
    app.add_option(
        "--height",
        args.height,
        "Rasterization texture height. Defaults to 1024 if no base texture is provided.");
    app.add_option(
        "--low-confidence-ratio",
        args.low_confidence_ratio,
        "Discard low confidence texels whose weights are < ratio * max_weight.");
    app.add_option("-l,--level", args.log_level, "Log level (0 = most verbose, 6 = off).");

    CLI11_PARSE(app, argc, argv)
    spdlog::set_level(static_cast<spdlog::level::level_enum>(args.log_level));

    // Load input scene
    lagrange::logger().info("Loading input scene: {}", args.input_scene.string());
    lagrange::io::LoadOptions load_options;
    load_options.stitch_vertices = true;
    auto scene =
        lagrange::io::load_scene<lagrange::scene::Scene32f>(args.input_scene, load_options);

    // Extract mesh, base texture, and cameras from scene
    auto [mesh, scene_base_texture] = lagrange::scene::internal::single_mesh_from_scene(scene);
    const auto cameras = lagrange::scene::internal::camera_transforms_from_scene(scene);
    lagrange::logger().info("Found {} cameras in the input scene", cameras.size());

    // Load (optional) base texture — overrides any texture embedded in the scene
    std::optional<Array3Df> base_texture;
    if (!args.input_texture.empty()) {
        lagrange::logger().info("Loading base texture: {}", args.input_texture.string());
        base_texture = load_image(args.input_texture);
        if (scene_base_texture.has_value()) {
            lagrange::logger().warn(
                "Input scene already contains a base texture. Overriding with user-provided "
                "texture.");
        }
    } else {
        base_texture = std::move(scene_base_texture);
    }

    la_runtime_assert(
        !args.input_renders.empty() || !args.input_render_grid.empty(),
        "Either --renders-in or --render-grid-in must be provided");
    la_runtime_assert(
        args.input_renders.empty() || args.input_render_grid.empty(),
        "--renders-in and --render-grid-in are mutually exclusive");

    // `render_grid` and `renders` own the image data referenced by `views`.
    Array3Df render_grid;
    std::vector<Array3Df> renders;
    std::vector<ConstView3Df> views;
    if (!args.input_render_grid.empty()) {
        lagrange::logger().info("Loading render grid: {}", args.input_render_grid.string());
        render_grid = load_image(args.input_render_grid);
        la_runtime_assert(!cameras.empty(), "No cameras found in the input scene");
        views = lagrange::image::experimental::split_grid(
            ConstView3Df(render_grid.to_mdspan()),
            {cameras.size()});
    } else {
        sort_paths(args.input_renders);
        lagrange::logger().info("Loading input {} renders", args.input_renders.size());
        for (const auto& render : args.input_renders) {
            renders.push_back(load_image(render));
            views.push_back(renders.back().to_mdspan());
        }
    }

    auto textures_and_weights = lagrange::texproc::rasterize_textures_from_renders(
        mesh,
        std::move(base_texture),
        cameras,
        views,
        args.width,
        args.height,
        args.low_confidence_ratio,
        args.base_confidence);

    // Save textures and confidences
    tbb::parallel_for(size_t(0), textures_and_weights.size(), [&](size_t i) {
        fs::path output_texture = make_output_path(args.output_textures, i);
        fs::path output_weight = make_output_path(args.output_weights, i);
        lagrange::logger().info("Saving texture: {}", output_texture.string());
        save_image(output_texture, textures_and_weights[i].first.to_mdspan());
        lagrange::logger().info("Saving confidence: {}", output_weight.string());
        save_image(output_weight, textures_and_weights[i].second.to_mdspan());
    });

    // Save textured meshes (one per view)
    if (!args.output_meshes.empty()) {
        auto mesh = lagrange::scene::scene_to_mesh(scene);
        lagrange::io::SaveOptions save_options;
        save_options.encoding = lagrange::io::FileEncoding::Binary;
        save_options.embed_images = true;
        save_options.attribute_conversion_policy =
            lagrange::io::SaveOptions::AttributeConversionPolicy::ConvertAsNeeded;
        for (size_t i = 0; i < textures_and_weights.size(); ++i) {
            fs::path output_mesh = make_output_path(args.output_meshes, i);
            lagrange::logger().info("Saving textured mesh: {}", output_mesh.string());
            auto output_scene = lagrange::scene::internal::single_mesh_to_scene(
                mesh,
                textures_and_weights[i].first.to_mdspan());
            lagrange::io::save_scene(output_mesh, output_scene, save_options);
        }
    }

    return 0;
}

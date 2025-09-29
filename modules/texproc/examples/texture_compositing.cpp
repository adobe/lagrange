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
#include "io_helpers.h"
#include "../shared/shared_utils.h"

#include <lagrange/io/load_mesh.h>
#include <lagrange/texproc/texture_compositing.h>
#include <lagrange/utils/assert.h>

#include <CLI/CLI.hpp>

namespace fs = lagrange::fs;

template <typename ValueType>
lagrange::image::experimental::View3D<ValueType> extract_channel(
    lagrange::image::experimental::View3D<ValueType> image,
    size_t channel)
{
    return submdspan(
        image,
        lagrange::image::experimental::full_extent_t(),
        lagrange::image::experimental::full_extent_t(),
        std::tuple{channel, 1});
}

int main(int argc, char** argv)
{
    struct
    {
        fs::path input_mesh;
        std::vector<fs::path> input_textures;
        std::vector<fs::path> input_weights;
        fs::path output_texture = "output.exr";
        int log_level = 2;
    } args;
    lagrange::texproc::CompositingOptions compositing_options;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("--mesh-in", args.input_mesh, "Input mesh with UVs.")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("--textures-in", args.input_textures, "Input textures images.")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("--weights-in", args.input_weights, "Input weights images.")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("--texture-out", args.output_texture, "Output texture.");
    app.add_option(
        "--value-weight",
        compositing_options.value_weight,
        "Value interpolation weight.");
    app.add_option(
        "--quadrature",
        compositing_options.quadrature_samples,
        "Number of quadrature samples (in {1, 3, 6, 12, 24, 32}.");
    app.add_option(
        "--jitter-epsilon",
        compositing_options.jitter_epsilon,
        "Random jitter amount (0 if no jittering).");
    app.add_flag(
        "--smooth-low-weight-areas",
        compositing_options.smooth_low_weight_areas,
        "Smooth pixels with low total weight (< 1).");
    app.add_option("-l,--level", args.log_level, "Log level (0 = most verbose, 6 = off).");

    CLI11_PARSE(app, argc, argv)
    spdlog::set_level(static_cast<spdlog::level::level_enum>(args.log_level));

    // Sort input textures and weights
    sort_paths(args.input_textures);
    sort_paths(args.input_weights);

    lagrange::logger().info("Loading input mesh: {}", args.input_mesh.string());
    lagrange::io::LoadOptions load_options;
    load_options.stitch_vertices = true;
    auto mesh = lagrange::io::load_mesh<lagrange::SurfaceMesh32f>(args.input_mesh, load_options);

    lagrange::logger().info("Compositing {} textures", args.input_textures.size());
    std::vector<Array3Df> textures;
    std::vector<Array3Df> weights;
    std::vector<lagrange::texproc::ConstWeightedTextureView<float>> weighted_textures;
    la_runtime_assert(
        args.input_textures.size() == args.input_weights.size(),
        "Number of textures and weights images must be the same");
    for (size_t i = 0; i < args.input_textures.size(); ++i) {
        textures.emplace_back(load_image(args.input_textures[i]));
        weights.emplace_back(load_image(args.input_weights[i]));
        weighted_textures.push_back({
            textures.back().to_mdspan(),
            extract_channel(weights.back().to_mdspan(), 0),
        });
    }
    auto image =
        lagrange::texproc::texture_compositing(mesh, weighted_textures, compositing_options);

    lagrange::logger().info("Saving result: {}", args.output_texture.string());
    save_image(args.output_texture, image.to_mdspan());

    return 0;
}

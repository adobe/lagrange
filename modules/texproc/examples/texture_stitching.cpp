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

#include <lagrange/io/load_mesh.h>
#include <lagrange/texproc/texture_stitching.h>

#include <CLI/CLI.hpp>

namespace fs = lagrange::fs;

int main(int argc, char** argv)
{
    struct
    {
        fs::path input_mesh;
        fs::path input_texture;
        fs::path output_texture = "output.exr";
        int log_level = 2;
    } args;
    lagrange::texproc::StitchingOptions stitching_options;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("--mesh-in", args.input_mesh, "Input mesh with UVs.")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("--texture-in", args.input_texture, "Input texture.")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("--texture-out", args.output_texture, "Output texture.");
    app.add_option(
        "--exterior-only",
        stitching_options.exterior_only,
        "Only exterior boundary texels update.");
    app.add_option(
        "--quadrature",
        stitching_options.quadrature_samples,
        "Number of quadrature samples (in {1, 3, 6, 12, 24, 32}.");
    app.add_option(
        "--jitter-epsilon",
        stitching_options.jitter_epsilon,
        "Random jitter amount (0 if no jittering).");
    app.add_option("-l,--level", args.log_level, "Log level (0 = most verbose, 6 = off).");

    CLI11_PARSE(app, argc, argv)
    spdlog::set_level(static_cast<spdlog::level::level_enum>(args.log_level));

    lagrange::logger().info("Loading input mesh: {}", args.input_mesh.string());
    lagrange::io::LoadOptions load_options;
    load_options.stitch_vertices = true;
    auto mesh = lagrange::io::load_mesh<lagrange::SurfaceMesh32f>(args.input_mesh, load_options);

    lagrange::logger().info("Loading input texture: {}", args.input_texture.string());
    Array3Df image = load_image(args.input_texture);

    lagrange::logger().info("Running texture stitching");
    lagrange::texproc::texture_stitching(mesh, image.to_mdspan(), stitching_options);

    lagrange::logger().info("Saving result: {}", args.output_texture.string());
    save_image(args.output_texture, image.to_mdspan());

    return 0;
}

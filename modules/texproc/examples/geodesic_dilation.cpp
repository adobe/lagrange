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
#include <lagrange/io/save_mesh.h>
#include <lagrange/texproc/geodesic_dilation.h>

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
        int posmap_width = 1024;
        int posmap_height = 1024;
    } args;

    lagrange::texproc::DilationOptions dilation_options;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("--mesh-in", args.input_mesh, "Input mesh with UVs.")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("--texture-in", args.input_texture, "Input texture.")->check(CLI::ExistingFile);
    app.add_option("--texture-out", args.output_texture, "Output texture.");
    app.add_option(
        "-d,--dilation-radius",
        dilation_options.dilation_radius,
        "Texture dilation radius.");
    app.add_flag(
        "--position-map",
        dilation_options.output_position_map,
        "Output a position map instead of a texture.");
    app.add_option("-W,--width", args.posmap_width, "Position map width.");
    app.add_option("-H,--height", args.posmap_height, "Position map height.");
    app.add_option("-l,--level", args.log_level, "Log level (0 = most verbose, 6 = off).");

    CLI11_PARSE(app, argc, argv)
    spdlog::set_level(static_cast<spdlog::level::level_enum>(args.log_level));

    lagrange::logger().info("Loading input mesh: {}", args.input_mesh.string());
    lagrange::io::LoadOptions load_options;
    load_options.stitch_vertices = true;
    auto mesh = lagrange::io::load_mesh<lagrange::SurfaceMesh32f>(args.input_mesh, load_options);

    auto image = [&]() -> Array3Df {
        if (dilation_options.output_position_map) {
            lagrange::logger().info("Creating new geodesic position texture");
            return lagrange::image::experimental::create_image<float>(args.posmap_width, args.posmap_height, 3);
        } else {
            lagrange::logger().info("Loading input texture: {}", args.input_texture.string());
            return load_image(args.input_texture);
        }
    }();

    lagrange::logger().info("Running geodesic texture dilation");
    lagrange::texproc::geodesic_dilation(mesh, image.to_mdspan(), dilation_options);

    lagrange::logger().info("Saving result: {}", args.output_texture.string());
    save_image(args.output_texture, image.to_mdspan());

    return 0;
}

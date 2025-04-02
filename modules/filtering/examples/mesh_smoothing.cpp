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
#include <lagrange/filtering/mesh_smoothing.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_mesh.h>

#include <CLI/CLI.hpp>

int main(int argc, char** argv)
{
    struct
    {
        std::string input;
        std::string output = "output.obj";
        int log_level = 2; // normal
    } args;

    lagrange::filtering::SmoothingOptions smooth_options;

    CLI::App app{argv[0]};
    app.add_option("input", args.input, "Input point cloud.")->required()->check(CLI::ExistingFile);
    app.add_option("output", args.output, "Output mesh.");
    app.add_option("--curvature-weight", smooth_options.curvature_weight, "Curvature weight.");
    app.add_option(
        "--normal-smoothing-weight",
        smooth_options.normal_smoothing_weight,
        "Normal smoothing weight.");
    app.add_option(
        "--gradient-scale",
        smooth_options.gradient_modulation_scale,
        "Gradient modulation scale.");
    app.add_option(
        "--gradient-weight",
        smooth_options.gradient_weight,
        "Modulated gradient fitting weight.");
    app.add_option(
        "--normal-projection-weight",
        smooth_options.normal_projection_weight,
        "Normal projection weight.");
    app.add_option("-l,--level", args.log_level, "Log level (0 = most verbose, 6 = off).");

    CLI11_PARSE(app, argc, argv)
    spdlog::set_level(static_cast<spdlog::level::level_enum>(args.log_level));

    lagrange::logger().info("Loading input mesh: {}", args.input);
    auto mesh = lagrange::io::load_mesh<lagrange::SurfaceMesh32f>(args.input);

    lagrange::logger().info("Running mesh smoothing");
    lagrange::filtering::mesh_smoothing(mesh, smooth_options);

    lagrange::logger().info("Saving result: {}", args.output);
    lagrange::io::save_mesh(args.output, mesh);

    return 0;
}

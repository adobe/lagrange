/*
 * Copyright 2020 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/common.h>
#include <lagrange/internal/constants.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/primitive/generate_torus.h>

#include <CLI/CLI.hpp>

int main(int argc, char** argv)
{
    struct
    {
        std::string output;
        lagrange::primitive::TorusOptions setting;
        double start_sweep_degree = 0;
        double end_sweep_degree = 360;
    } args;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("output", args.output, "Output mesh.")->required();
    app.add_option("-R,--major-radius", args.setting.major_radius, "Major radius.")->required();
    app.add_option("-r,--minor-radius", args.setting.minor_radius, "Minor radius.")->required();
    app.add_option(
        "-p,--pipe",
        args.setting.pipe_segments,
        "Number of segments along the pipe direction.");
    app.add_option(
        "-g,--ring",
        args.setting.ring_segments,
        "Number of segments along the ring direction.");
    app.add_option("-s,--start-sweep", args.start_sweep_degree, "Start sweep angle in degrees.");
    app.add_option("-e,--end-sweep", args.end_sweep_degree, "End sweep angle in degrees.");
    app.add_flag("!--no-caps", args.setting.with_top_cap, "Do not generate caps.");
    app.add_flag(
        "-t,--triangulate",
        args.setting.triangulate,
        "Triangulate the generated surface.");
    app.add_flag("--fixed-uv", args.setting.fixed_uv, "Use fixed UV coordinates.");
    CLI11_PARSE(app, argc, argv)

    args.setting.start_sweep_angle =
        static_cast<float>(args.start_sweep_degree * lagrange::internal::pi / 180.0);
    args.setting.end_sweep_angle =
        static_cast<float>(args.end_sweep_degree * lagrange::internal::pi / 180.0);
    args.setting.dist_threshold = 1e-3f; // distance threshold used for merging vertices.
    args.setting.with_bottom_cap = args.setting.with_top_cap;

    auto mesh = lagrange::primitive::generate_torus<double, uint32_t>(std::move(args.setting));
    lagrange::io::save_mesh(args.output, mesh);
    return 0;
}

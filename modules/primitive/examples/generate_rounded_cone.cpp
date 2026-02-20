/*
 * Copyright 2019 Adobe. All rights reserved.
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
#include <lagrange/primitive/generate_rounded_cone.h>

#include <CLI/CLI.hpp>

int main(int argc, char** argv)
{
    struct
    {
        std::string output;
        lagrange::primitive::RoundedConeOptions setting;
        double start_sweep_degree = 0;
        double end_sweep_degree = 360;
    } args;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("output", args.output, "Output mesh.")->required();
    app.add_option("--top-r", args.setting.radius_top, "Top radius.")->required();
    app.add_option("--bot-r", args.setting.radius_bottom, "Bottom radius.")->required();
    app.add_option("-H,--height", args.setting.height, "Height.")->required();
    app.add_option("--top-bevel", args.setting.bevel_radius_top, "Top bevel.");
    app.add_option("--bot-bevel", args.setting.bevel_radius_bottom, "Bottom bevel.");
    app.add_option("--start-sweep", args.start_sweep_degree, "Start sweep angle in degrees.");
    app.add_option("--end-sweep", args.end_sweep_degree, "End sweep angle in degress.");
    app.add_option("--radial-segments", args.setting.radial_sections, "Number of radial segments.");
    app.add_option(
        "--bevel-segments-top",
        args.setting.bevel_segments_top,
        "Number of bevel segments at top.");
    app.add_option(
        "--bevel-segments-bottom",
        args.setting.bevel_segments_bottom,
        "Number of bevel segments at bottom.");
    app.add_option("--side-segments", args.setting.side_segments, "Number of side segments.");
    app.add_flag(
        "!--no-cross-section",
        args.setting.with_cross_section,
        "Disable cross section generation.");
    app.add_flag("!--no-top-cap", args.setting.with_top_cap, "Disable top cap generation.");
    app.add_flag(
        "!--no-bottom-cap",
        args.setting.with_bottom_cap,
        "Disable bottom cap generation.");
    app.add_flag("--fixed-uv", args.setting.fixed_uv, "Use fixed UV coordinates.");
    app.add_flag("--triangulate", args.setting.triangulate, "Triangulate the mesh.");
    CLI11_PARSE(app, argc, argv)

    args.setting.start_sweep_angle = args.start_sweep_degree * lagrange::internal::pi / 180.0;
    args.setting.end_sweep_angle = args.end_sweep_degree * lagrange::internal::pi / 180.0;

    using Scalar = float;
    using Index = uint32_t;
    auto mesh = lagrange::primitive::generate_rounded_cone<Scalar, Index>(std::move(args.setting));
    lagrange::io::save_mesh(args.output, mesh);

    return 0;
}

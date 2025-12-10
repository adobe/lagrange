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
#include <lagrange/internal/constants.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/primitive/generate_sphere.h>
#include <lagrange/utils/assert.h>

#include <CLI/CLI.hpp>

int main(int argc, char** argv)
{
    struct
    {
        std::string output;
        lagrange::primitive::SphereOptions setting;
        double start_sweep_angle_degree = 0;
        double end_sweep_angle_degree = 360;
    } args;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("output", args.output, "Output mesh.")->required();
    app.add_option("-r,--radius", args.setting.radius, "Radius.")->required();
    app.add_option(
        "--start-sweep-angle",
        args.start_sweep_angle_degree,
        "Start sweep angle in degree.");
    app.add_option("--end-sweep-angle", args.end_sweep_angle_degree, "End sweep angle in degree.");
    app.add_option(
        "--num-longitude-sections",
        args.setting.num_longitude_sections,
        "Number of segments.");
    app.add_option(
        "--num-latitude-sections",
        args.setting.num_latitude_sections,
        "Number of segments.");
    app.add_flag(
        "!--no-cross-section",
        args.setting.with_cross_section,
        "Do not generate sweep cross sections.");
    app.add_flag("--fixed-uv", args.setting.fixed_uv, "Generate fixed UV coordinates.");
    app.add_flag("--triangulate", args.setting.triangulate, "Triangulate the mesh.");
    CLI11_PARSE(app, argc, argv)

    args.setting.start_sweep_angle = args.start_sweep_angle_degree * lagrange::internal::pi / 180.0;
    args.setting.end_sweep_angle = args.end_sweep_angle_degree * lagrange::internal::pi / 180.0;

    auto mesh = lagrange::primitive::generate_sphere<float, uint32_t>(std::move(args.setting));
    lagrange::io::save_mesh(args.output, mesh);
    return 0;
}

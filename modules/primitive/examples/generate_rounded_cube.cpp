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
#include <lagrange/io/save_mesh.h>
#include <lagrange/primitive/generate_rounded_cube.h>

#include <CLI/CLI.hpp>

int main(int argc, char** argv)
{
    struct
    {
        std::string output;
        lagrange::primitive::RoundedCubeOptions setting;
    } args;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("output", args.output, "Output mesh.")->required();
    app.add_option("-W,--width", args.setting.width, "Width of the cube.");
    app.add_option("-H,--height", args.setting.height, "Height of the cube.");
    app.add_option("-D,--depth", args.setting.depth, "Depth of the cube.");
    app.add_option("-r,--bevel-radius", args.setting.bevel_radius, "Bevel radius.");
    app.add_option("--bevel-segments", args.setting.bevel_segments, "Number of bevel segments.");
    app.add_option("--width-segments", args.setting.width_segments, "Number of width segments.");
    app.add_option("--height-segments", args.setting.height_segments, "Number of height segments.");
    app.add_option("--depth-segments", args.setting.depth_segments, "Number of depth segments.");
    app.add_flag("-t,--triangulate", args.setting.triangulate, "Triangulate the mesh.");
    app.add_flag("--fixed-uv", args.setting.fixed_uv, "Use fixed UV mode.");
    CLI11_PARSE(app, argc, argv)

    using Scalar = float;
    using Index = uint32_t;
    auto mesh = lagrange::primitive::generate_rounded_cube<Scalar, Index>(args.setting);
    lagrange::io::save_mesh(args.output, mesh);
    return 0;
}

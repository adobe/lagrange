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
#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/image_io/save_image_svg.h>
#include <lagrange/io/load_mesh.h>

#include <CLI/CLI.hpp>

int main(int argc, char const* argv[])
{
    struct
    {
        std::string input;
        std::string output;
        lagrange::image_io::SVGSetting settings;
    } args;

    CLI::App app{"Convert 2D mesh into a SVG image"};
    app.add_option("input", args.input, "Input mesh.");
    app.add_option("output", args.output, "Output svg image.");
    app.add_flag("--uv", args.settings.use_uv_mesh, "Use UV coordinate of the mesh.");
    app.add_flag("--with-stroke,!--no-stroke", args.settings.with_stroke, "Stroke triangles.");
    app.add_flag("--with-fill,!--no-fill", args.settings.with_fill, "Fill triangles.");
    app.add_option(
        "--scaling,-s",
        args.settings.scaling_factor,
        "Scale the output by this amount.");
    app.add_option("--stroke-color", args.settings.stroke_color, "Stroke color.");
    app.add_option("--fill-color", args.settings.fill_color, "Fill color.");
    app.add_option("--stroke-width", args.settings.stroke_width, "Stroke width");
    CLI11_PARSE(app, argc, argv);

    if (args.settings.use_uv_mesh) {
        args.settings.width = args.settings.scaling_factor;
        args.settings.height = args.settings.scaling_factor;
    }

    using MeshType = lagrange::TriangleMesh2D;
    auto mesh = lagrange::io::load_mesh<MeshType>(args.input);
    lagrange::image_io::save_image_svg(args.output, *mesh, args.settings);

    return 0;
}

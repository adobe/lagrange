/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/orient_outward.h>

#include <CLI/CLI.hpp>

int main(int argc, char** argv)
{
    using Scalar = float;
    using Index = uint32_t;
    using Mesh = lagrange::SurfaceMesh<Scalar, Index>;

    struct
    {
        std::string input;
        std::string output = "output.obj";
    } args;
    lagrange::OrientOptions options;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("input", args.input, "Input mesh.")->required()->check(CLI::ExistingFile);
    app.add_option("output", args.output, "Output mesh.");
    app.add_option(
        "-p,--positive",
        options.positive,
        "Whether to orient each volume positively or negatively.");
    CLI11_PARSE(app, argc, argv)

    lagrange::logger().set_level(spdlog::level::trace);

    lagrange::logger().info("Loading input mesh: {}", args.input);
    auto mesh = lagrange::io::load_mesh<Mesh>(args.input);

    lagrange::logger().info("Orienting facets ({})", options.positive ? "positive" : "negative");
    lagrange::orient_outward(mesh, options);

    lagrange::logger().info("Saving result: {}", args.output);
    lagrange::io::save_mesh(args.output, mesh);

    return 0;
}

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
#include <lagrange/bvh/remove_interior_shells.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/utils/timing.h>

#include <CLI/CLI.hpp>

int main(int argc, char** argv)
{
    struct
    {
        std::string input;
        std::string output = "output.ply";
        int log_level = 1; // debug
    } args;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("input", args.input, "Input mesh.")->required()->check(CLI::ExistingFile);
    app.add_option("output", args.output, "Output mesh.");
    app.add_option("-l,--level", args.log_level, "Log level (0 = most verbose, 6 = off).");
    CLI11_PARSE(app, argc, argv)

    args.log_level = std::max(0, std::min(6, args.log_level));
    spdlog::set_level(static_cast<spdlog::level::level_enum>(args.log_level));

    lagrange::logger().info("Loading input mesh: {}", args.input);
    auto mesh = lagrange::io::load_mesh<lagrange::SurfaceMesh32d>(args.input);

    lagrange::logger().info("Triangulating mesh...");
    lagrange::triangulate_polygonal_facets(mesh);

    lagrange::logger().info(
        "Mesh has {} vertices and {} facets.",
        mesh.get_num_vertices(),
        mesh.get_num_facets());

    lagrange::logger().info("Removing interior shells...");
    lagrange::VerboseTimer timer("remove_interior_shells");
    timer.tick();
    mesh = lagrange::bvh::remove_interior_shells(mesh);
    timer.tock();

    lagrange::logger().info("Saving result: {}", args.output);
    lagrange::io::save_mesh(args.output, mesh);

    return 0;
}

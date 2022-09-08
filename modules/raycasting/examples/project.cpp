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
////////////////////////////////////////////////////////////////////////////////
#include <lagrange/Mesh.h>
#include <lagrange/attributes/attribute_utils.h>
#include <lagrange/create_mesh.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/raycasting/project_attributes.h>
#include <lagrange/utils/timing.h>

#include <spdlog/sinks/basic_file_sink.h>
#include <CLI/CLI.hpp>
////////////////////////////////////////////////////////////////////////////////

// Default arguments
struct Args
{
    std::string source;
    std::string target;
    std::string output = "output.obj";
    std::vector<std::string> attributes;
    std::string log_file;
    bool quiet = false;
    int log_level = 2; // normal

    // Projection options
    lagrange::raycasting::ProjectMode project_mode;
    lagrange::raycasting::WrapMode wrap_mode = lagrange::raycasting::WrapMode::CONSTANT;
    lagrange::raycasting::CastMode cast_mode = lagrange::raycasting::CastMode::BOTH_WAYS;
    std::array<double, 3> direction = {0, 0, 1};
    double fill_value = 0.0;
};

int parse_args(int argc, char const* argv[], Args& args)
{
    // Parse arguments
    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("source,-s,--source", args.source, "Source mesh to transfer from.")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("target,-t,--target", args.target, "Target mesh to transfer to.")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("output,-o,--output", args.output, "Output mesh with attributes transfered.");
    app.add_option("-a,--attributes", args.attributes, "Names of the attributes to transfer.");

    // Projection options
    app.add_option(
           "--project-mode",
           args.project_mode,
           "Projection mode used to transfer attributes.")
        ->transform(
            CLI::CheckedTransformer(lagrange::raycasting::project_modes(), CLI::ignore_case));
    app.add_option(
           "--wrap-mode",
           args.wrap_mode,
           "Wrapping mode for non-hit vertices when using ray-casting   projection mode.")
        ->transform(CLI::CheckedTransformer(lagrange::raycasting::wrap_modes(), CLI::ignore_case));
    app.add_option(
           "--cast-mode",
           args.cast_mode,
           "Ray-casting mode (forward, or both forward and backward), when using ray-casting "
           "projection mode")
        ->transform(CLI::CheckedTransformer(lagrange::raycasting::cast_modes(), CLI::ignore_case));
    app.add_option("--direction", args.direction, "Ray direction for ray-casting projection mode.");
    app.add_option(
        "--fill",
        args.fill_value,
        "Fill value for non-hit vertices when using ray-casting projection mode with constant wrap "
        "mode.");

    // Logging options
    auto lgroup = app.add_option_group("Logging", "Options for the logger");
    lgroup->option_defaults()->always_capture_default();
    lgroup->add_flag("-q,--quiet", args.quiet, "Hide logger on stdout.");
    lgroup->add_option("-l,--level", args.log_level, "Log level (0 = most verbose, 6 = off).");
    lgroup->add_option("-f,--log-file", args.log_file, "Log file.");
    CLI11_PARSE(app, argc, argv)

    return 0;
}

int main(int argc, char const* argv[])
{
    Args args;
    if (int ret = parse_args(argc, argv, args)) {
        return ret;
    }

    // Global spdlog behavior, set by the client
    args.log_level = std::max(0, std::min(6, args.log_level));
    if (args.quiet) {
        lagrange::logger().sinks().clear();
    }
    if (!args.log_file.empty()) {
        lagrange::logger().sinks().emplace_back(
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(args.log_file, true));
    }
    spdlog::set_level(static_cast<spdlog::level::level_enum>(args.log_level));
    spdlog::flush_every(std::chrono::seconds(3));

    // Mesh data
    lagrange::logger().info("Loading models...");
    auto source = lagrange::io::load_mesh<lagrange::TriangleMesh3D>(args.source);
    auto target = lagrange::io::load_mesh<lagrange::TriangleMesh3D>(args.target);

    // Sanity check
    bool ok = true;
    for (const auto& name : args.attributes) {
        if (name == "uv") {
            if (!source->is_uv_initialized() && !source->has_vertex_attribute(name)) {
                lagrange::logger().warn("Source mesh does not have any uv");
                ok = false;
            } else if (!source->is_uv_initialized()) {
                lagrange::logger().debug("Mapping indexed attribute to vetex attribute for uv.");
                lagrange::map_indexed_attribute_to_vertex_attribute(*source, name);
            }
        } else {
            if (!source->has_vertex_attribute(name)) {
                lagrange::logger().warn("Source mesh does not have vertex attribute: {}", name);
                ok = false;
            }
        }
    }
    if (!ok) {
        return 0;
    }

    // Project stuff
    Eigen::Vector3d dir(args.direction[0], args.direction[1], args.direction[2]);
    auto start_time = lagrange::get_timestamp();
    lagrange::raycasting::project_attributes(
        *source,
        *target,
        args.attributes,
        args.project_mode,
        dir,
        args.cast_mode,
        args.wrap_mode,
        args.fill_value);
    auto finish_time = lagrange::get_timestamp();
    auto timing = lagrange::timestamp_diff_in_seconds(start_time, finish_time);
    lagrange::logger().info("Projection running time: {:.3f} s", timing);

    // Write output
    lagrange::io::save_mesh(args.output, *target);

    return 0;
}

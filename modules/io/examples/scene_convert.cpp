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
#include <lagrange/Logger.h>
#include <lagrange/io/load_scene.h>
#include <lagrange/io/save_scene.h>
#include <lagrange/mesh_cleanup/remove_duplicate_vertices.h>
#include <lagrange/utils/strings.h>

#include <CLI/CLI.hpp>

int main(int argc, char** argv)
{
    using namespace lagrange;
    using Scalar = float;
    using Index = uint32_t;
    using SceneType = scene::Scene<Scalar, Index>;

    struct
    {
        std::string input;
        std::string output;
        bool verbose = false;
    } args;

    lagrange::logger().set_level(spdlog::level::info);

    CLI::App app{
        "Scene format conversion tool - loads any supported scene format and saves to any "
        "supported format",
        argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option(
           "input",
           args.input,
           "Input scene file. Supported formats: .gltf, .glb, .fbx, .obj (and others if Assimp is "
           "enabled).")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option(
           "output",
           args.output,
           "Output scene file. Supported formats: .gltf, .glb, .obj.")
        ->required();
    app.add_flag("-v,--verbose", args.verbose, "Verbose output.");
    CLI11_PARSE(app, argc, argv)

    if (args.verbose) {
        lagrange::logger().set_level(spdlog::level::debug);
    }

    // Get file extensions for informational purposes
    std::string input_ext = to_lower(fs::path(args.input).extension().string());
    std::string output_ext = to_lower(fs::path(args.output).extension().string());


    // Load scene
    lagrange::logger().info("Loading scene: {}", args.input);
    auto scene = io::load_scene<SceneType>(args.input);

    // Display scene info
    lagrange::logger().info(
        "Loaded scene '{}' with {} meshes, {} nodes, {} materials",
        scene.name.empty() ? "(unnamed)" : scene.name,
        scene.meshes.size(),
        scene.nodes.size(),
        scene.materials.size());

    // Save scene
    lagrange::logger().info("Saving scene: {}", args.output);
    io::SaveOptions save_options;
    save_options.encoding = io::FileEncoding::Ascii;
    io::save_scene(args.output, scene, save_options);

    lagrange::logger().info("Conversion completed successfully!");

    return 0;
}

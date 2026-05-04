/*
 * Copyright 2023 Adobe. All rights reserved.
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
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/load_simple_scene.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/io/save_simple_scene.h>
#include <lagrange/mesh_cleanup/remove_duplicate_vertices.h>
#include <lagrange/scene/simple_scene_convert.h>
#include <lagrange/utils/strings.h>

#include <CLI/CLI.hpp>

#include <unordered_set>

using Index = uint32_t;
constexpr size_t Dimension = 3;

std::unordered_set<std::string_view> input_scene_formats()
{
    return {".gltf", ".glb", ".fbx", ".lgs"};
}
std::unordered_set<std::string_view> output_scene_formats()
{
    return {".gltf", ".glb", ".obj", ".lgs"};
}

template <typename Scalar>
void convert(const lagrange::fs::path& input_filename, const lagrange::fs::path& output_filename)
{
    using MeshType = lagrange::SurfaceMesh<Scalar, Index>;
    using SceneType = lagrange::scene::SimpleScene<Scalar, Index, Dimension>;

    // Load as scene if extension is .fbx, .gtlf or .glb
    std::string input_ext = lagrange::to_lower(input_filename.extension().string());
    if (input_scene_formats().count(input_ext)) {
        // Load scene
        lagrange::logger().info("Loading input scene: {}", input_filename.string());
        lagrange::io::LoadOptions load_options;
        if (input_ext == ".gltf" || input_ext == ".glb") {
            // If input scene if gtTF, we need to stitch duplicate vertices (glTF doesn't support
            // indexed buffers)
            load_options.stitch_vertices = true;
        }
        auto scene = lagrange::io::load_simple_scene<SceneType>(input_filename, load_options);

        // Display info
        lagrange::logger().info(
            "Input scene has {} meshes and {} instances",
            scene.get_num_meshes(),
            scene.compute_num_instances());

        // Save as scene or mesh
        std::string output_ext = lagrange::to_lower(output_filename.extension().string());
        if (output_scene_formats().count(output_ext)) {
            lagrange::logger().info("Saving output scene: {}", output_filename.string());
            lagrange::io::save_simple_scene(output_filename, scene);
        } else {
            lagrange::logger().info("Saving output mesh: {}", output_filename.string());
            lagrange::TransformOptions options;
            options.reorient = true;
            lagrange::io::save_mesh(
                output_filename,
                lagrange::scene::simple_scene_to_mesh(scene, options));
        }
    } else {
        // Load mesh
        lagrange::logger().info("Loading input mesh: {}", input_filename.string());
        auto mesh = lagrange::io::load_mesh<MeshType>(input_filename);

        // Display info
        lagrange::logger().info(
            "Input mesh has {} vertices and {} faces",
            mesh.get_num_vertices(),
            mesh.get_num_facets());

        // Save mesh
        lagrange::logger().info("Saving output mesh: {}", output_filename.string());
        lagrange::io::save_mesh(output_filename, mesh);
    }
}

int main(int argc, char** argv)
{
    struct
    {
        std::string input;
        std::string output;
        bool use_float = true;
    } args;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("input", args.input, "Input mesh.")->required()->check(CLI::ExistingFile);
    app.add_option("output", args.output, "Output mesh.")->required();
    app.add_option("--float", args.use_float, "Use float precision.");
    CLI11_PARSE(app, argc, argv)

    if (args.use_float) {
        convert<float>(args.input, args.output);
    } else {
        convert<double>(args.input, args.output);
    }

    return 0;
}

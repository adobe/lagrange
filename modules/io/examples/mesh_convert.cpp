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
#include <lagrange/combine_meshes.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/load_simple_scene.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/io/save_mesh_gltf.h>
#include <lagrange/views.h>

#include <CLI/CLI.hpp>

int main(int argc, char** argv)
{
    struct
    {
        std::string input;
        std::string output;
    } args;

    CLI::App app{argv[0]};
    app.add_option("input", args.input, "Input mesh.")->required()->check(CLI::ExistingFile);
    app.add_option("output", args.output, "Output mesh.")->required();
    CLI11_PARSE(app, argc, argv)

    lagrange::logger().set_level(spdlog::level::trace);

    using SceneType = lagrange::scene::SimpleScene32f3;
    using MeshType = typename SceneType::MeshType;

    // Load scene
    lagrange::logger().info("Loading input scene: {}", args.input);
    auto scene = lagrange::io::load_simple_scene<SceneType>(args.input);
    lagrange::logger().info(
        "Input scene has {} meshes and {} instances",
        scene.get_num_meshes(),
        scene.compute_num_instances());

    // Combine meshes
    std::vector<MeshType> meshes;
    scene.foreach_instances([&](auto instance) {
        MeshType mesh = scene.get_mesh(instance.mesh_index);
        auto vertices = vertex_ref(mesh);
        vertices = (instance.transform * vertices.leftCols<3>().transpose()).transpose();
        if (instance.transform.linear().determinant() < 0) {
            // TODO: We must flip facets due to transform with negative scale
        }
        meshes.emplace_back(std::move(mesh));
    });

    lagrange::logger().info("Combining {} meshes together", meshes.size());
    auto mesh = lagrange::combine_meshes(lagrange::span<const MeshType>(meshes));

    lagrange::logger().info("Saving output mesh: {}", args.output);
    lagrange::io::save_mesh(args.output, mesh);

    return 0;
}

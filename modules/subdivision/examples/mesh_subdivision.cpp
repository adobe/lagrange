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
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_mesh.h>

#include <lagrange/foreach_attribute.h>
#include <lagrange/map_attribute.h>
#include <lagrange/mesh_cleanup/remove_duplicate_vertices.h>
#include <lagrange/subdivision/mesh_subdivision.h>
#include <lagrange/subdivision/midpoint_subdivision.h>
#include <lagrange/subdivision/sqrt_subdivision.h>

#include <CLI/CLI.hpp>

int main(int argc, char** argv)
{
    struct
    {
        std::string input;
        std::string output = "output.obj";
        std::string scheme = "auto";
        bool output_btn = false;
    } args;

    lagrange::subdivision::SubdivisionOptions options;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("input", args.input, "Input mesh.")->required()->check(CLI::ExistingFile);
    app.add_option("output", args.output, "Output mesh.");
    app.add_option("-s,--scheme", args.scheme, "Subdivision scheme")
        ->check(CLI::IsMember({"auto", "bilinear", "loop", "catmark", "sqrt", "midpoint"}));
    app.add_option("-n,--num-levels", options.num_levels, "Number of subdivision levels");
    app.add_flag(
        "--limit",
        options.use_limit_surface,
        "Project vertex attributes to the limit surface");
    app.add_flag("--normal", args.output_btn, "Compute limit normal as a vertex attribute");
    CLI11_PARSE(app, argc, argv);

    lagrange::logger().set_level(spdlog::level::debug);

    lagrange::logger().info("Loading input mesh: {}", args.input);
    auto mesh = lagrange::io::load_mesh<lagrange::SurfaceMesh32d>(args.input);
    lagrange::logger().info(
        "Input mesh has {} vertices and {} facets",
        mesh.get_num_vertices(),
        mesh.get_num_facets());

    if ((std::set<std::string>{"auto", "bilinear", "loop", "catmark"}).count(args.scheme)) {
        // gltf meshes are unindexed, so vertices at UV seams are duplicated
        remove_duplicate_vertices(mesh);

        // Convert subdiv scheme to enum
        if (args.scheme == "loop") {
            options.scheme = lagrange::subdivision::SchemeType::Loop;
        } else if (args.scheme == "catmark") {
            options.scheme = lagrange::subdivision::SchemeType::CatmullClark;
        } else if (args.scheme == "bilinear") {
            options.scheme = lagrange::subdivision::SchemeType::Bilinear;
        }

        if (args.output_btn && mesh.has_attribute("normal")) {
            // Only output a single set of normals in this example
            mesh.delete_attribute("normal");
        }

        if (args.output_btn) {
            options.output_limit_normals = "normal";
        }
        mesh = lagrange::subdivision::subdivide_mesh(mesh, options);
        if (args.output_btn) {
            map_attribute_in_place(mesh, "normal", lagrange::AttributeElement::Indexed);
        }
    } else if (args.scheme == "sqrt") {
        for (unsigned i = 0; i < options.num_levels; ++i) {
            mesh = lagrange::subdivision::sqrt_subdivision(mesh);
            if (i + 1 < options.num_levels) {
                lagrange::logger().info(
                    "Intermediate mesh has {} vertices and {} facets",
                    mesh.get_num_vertices(),
                    mesh.get_num_facets());
            }
        }
    } else if (args.scheme == "midpoint") {
        for (unsigned i = 0; i < options.num_levels; ++i) {
            mesh = lagrange::subdivision::midpoint_subdivision(mesh);
            if (i + 1 < options.num_levels) {
                lagrange::logger().info(
                    "Intermediate mesh has {} vertices and {} facets",
                    mesh.get_num_vertices(),
                    mesh.get_num_facets());
            }
        }
    } else {
        throw std::runtime_error("Unsupported argument");
    }
    lagrange::logger().info(
        "Output mesh has {} vertices and {} facets",
        mesh.get_num_vertices(),
        mesh.get_num_facets());

    lagrange::logger().info("Saving output mesh: {}", args.output);
    lagrange::io::save_mesh(args.output, mesh);

    return 0;
}

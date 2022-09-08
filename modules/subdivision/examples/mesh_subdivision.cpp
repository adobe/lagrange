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
#include <lagrange/common.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_mesh.h>

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
        std::string scheme = "loop";
        int num_subdivisions = 1;
        int min_vertices = 0;
    } args;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("input", args.input, "Input mesh.")->required()->check(CLI::ExistingFile);
    app.add_option("output", args.output, "Output mesh.");
    app.add_option("-s,--scheme", args.scheme, "Subdivision scheme")
        ->check(CLI::IsMember({"loop", "catmark", "sqrt", "midpoint"}));
    app.add_option("-n,--num-subdivisions", args.num_subdivisions, "Number of subdivisions");
    app.add_option(
        "-m,--min-vertices",
        args.min_vertices,
        "Min number of vertices in the output mesh (sqrt and midpoint schemes only)");
    CLI11_PARSE(app, argc, argv);

    lagrange::logger().info("Loading input mesh: {}", args.input);
    auto mesh = lagrange::io::load_mesh<lagrange::TriangleMesh3D>(args.input);
    lagrange::logger().info(
        "Input mesh has {} vertices and {} facets",
        mesh->get_num_vertices(),
        mesh->get_num_facets());

    if (args.scheme == "loop" || args.scheme == "catmark") {
        auto scheme =
            (args.scheme == "loop" ? lagrange::subdivision::SubdivisionScheme::SCHEME_LOOP
                                   : lagrange::subdivision::SubdivisionScheme::SCHEME_CATMARK);
        mesh = lagrange::subdivision::subdivide_mesh<
            lagrange::TriangleMesh3D,
            lagrange::TriangleMesh3D>(*mesh, scheme, args.num_subdivisions);
    } else if (args.scheme == "sqrt") {
        for (int i = 0; i < args.num_subdivisions || mesh->get_num_vertices() < args.min_vertices;
             ++i) {
            mesh = lagrange::subdivision::sqrt_subdivision(*mesh);
            if (i + 1 < args.num_subdivisions) {
                lagrange::logger().info(
                    "Intermediate mesh has {} vertices and {} facets",
                    mesh->get_num_vertices(),
                    mesh->get_num_facets());
            }
        }
    } else if (args.scheme == "midpoint") {
        for (int i = 0; i < args.num_subdivisions || mesh->get_num_vertices() < args.min_vertices;
             ++i) {
            mesh = lagrange::subdivision::midpoint_subdivision(*mesh);
            if (i + 1 < args.num_subdivisions) {
                lagrange::logger().info(
                    "Intermediate mesh has {} vertices and {} facets",
                    mesh->get_num_vertices(),
                    mesh->get_num_facets());
            }
        }
    } else {
        throw std::runtime_error("Unsupported argument");
    }
    lagrange::logger().info(
        "Output mesh has {} vertices and {} facets",
        mesh->get_num_vertices(),
        mesh->get_num_facets());

    lagrange::logger().info("Saving output mesh: {}", args.output);
    lagrange::io::save_mesh(args.output, *mesh);

    return 0;
}

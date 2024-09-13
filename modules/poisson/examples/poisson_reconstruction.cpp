/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/Attribute.h>
#include <lagrange/cast_attribute.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/poisson/mesh_from_oriented_points.h>
#include <lagrange/utils/assert.h>

#include <CLI/CLI.hpp>

int main(int argc, char** argv)
{
    struct
    {
        std::string input;
        std::string output = "output.obj";
        bool dirichlet_boundaries = false;
        bool verbose = false;
        bool output_vertex_depth = false;
        unsigned int depth = 0;
    } args;

    CLI::App app{argv[0]};
    app.add_option("input", args.input, "Input point cloud.")->required()->check(CLI::ExistingFile);
    app.add_option("output", args.output, "Output mesh.");
    app.add_option("--depth", args.depth, "max reconstruction depth.");
    app.add_flag("--dirichlet", args.dirichlet_boundaries, "enable dirichlet bounary conditions.");
    app.add_flag("--verbose", args.verbose, "enable verbose output.");
    app.add_flag("--vertexDepth", args.output_vertex_depth, "enable outputting of vertex depth.");
    CLI11_PARSE(app, argc, argv)

    lagrange::logger().info("Loading input mesh: {}", args.input);
    auto oriented_points = lagrange::io::load_mesh<lagrange::SurfaceMesh32f>(args.input);

    lagrange::logger().info("Running Poisson surface reconstruction");
    lagrange::poisson::ReconstructionOptions recon_options;
    recon_options.verbose = args.verbose;
    recon_options.use_dirichlet_boundary = args.dirichlet_boundaries;
    recon_options.octree_depth = args.depth;
    if (args.output_vertex_depth) recon_options.output_vertex_depth_attribute_name = "value";

    if (auto id = find_matching_attribute(oriented_points, lagrange::AttributeUsage::Color);
        id.has_value()) {
        recon_options.interpolated_attribute_name = oriented_points.get_attribute_name(id.value());
    }

    auto mesh = lagrange::poisson::mesh_from_oriented_points(oriented_points, recon_options);

    lagrange::logger().info("Saving result: {}", args.output);
    lagrange::io::save_mesh(args.output, mesh);

    return 0;
}

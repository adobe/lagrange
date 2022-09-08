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
#include <lagrange/bvh/zip_boundary.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <igl/bounding_box_diagonal.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <CLI/CLI.hpp>
#include <Eigen/Core>

int main(int argc, char** argv)
{
    struct
    {
        std::string input;
        std::string output = "output.obj";
        double radius = -1e-3;
    } args;

    CLI::App app{argv[0]};
    app.add_option("input", args.input, "Input mesh.")->required()->check(CLI::ExistingFile);
    app.add_option("output", args.output, "Output mesh.");
    app.add_option("-r,--radius", args.radius, "Merging radius. < 0 means relative to the bbox diagonal");
    CLI11_PARSE(app, argc, argv)

    lagrange::logger().info("Loading input mesh: {}", args.input);
    auto mesh = lagrange::io::load_mesh<lagrange::TriangleMesh3D>(args.input);

    if (args.radius < 0) {
        double diag = igl::bounding_box_diagonal(mesh->get_vertices());
        lagrange::logger().info(
            "Using a relative tolerance of {:.3f} x {:.3f} = {:.3f}",
            std::abs(args.radius),
            diag,
            std::abs(args.radius) * diag);
        args.radius = std::abs(args.radius) * diag;
    }

    lagrange::logger().info("Welding vertices...");
    mesh = lagrange::bvh::zip_boundary(*mesh, args.radius);

    lagrange::logger().info("Saving result: {}", args.output);
    lagrange::io::save_mesh(args.output, *mesh);

    return 0;
}

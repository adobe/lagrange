/*
 * Copyright 2017 Adobe. All rights reserved.
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
#include <lagrange/mesh_cleanup/close_small_holes.h>
#include <lagrange/mesh_cleanup/remove_degenerate_triangles.h>
#include <lagrange/mesh_cleanup/remove_duplicate_vertices.h>
#include <lagrange/mesh_cleanup/split_long_edges.h>

#include <CLI/CLI.hpp>
#include <Eigen/Core>

Eigen::AlignedBox3d mesh_bbox(const lagrange::TriangleMesh3D& mesh)
{
    using Index = typename lagrange::TriangleMesh3D::Index;
    Eigen::AlignedBox3d bbox;
    la_runtime_assert(mesh.get_vertices().cols() == 3);
    for (Index v = 0; v < mesh.get_num_vertices(); ++v) {
        bbox.extend(mesh.get_vertices().row(v).transpose());
    }
    return bbox;
}

int main(int argc, char** argv)
{
    struct
    {
        std::string input;
        std::string output = "output.obj";
        double tol = 0.001;
        size_t max_holes = 0;
        bool holes_only = false;
        bool relative = true;
    } args;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("input", args.input, "Input mesh.")->required()->check(CLI::ExistingFile);
    app.add_option("output", args.output, "Output mesh.");
    app.add_option("-s,--max-holes", args.max_holes, "Max hole size to close.");
    app.add_flag("-H,--holes-only", args.holes_only, "Only fill holes.");
    app.add_option("-t,--tolerance", args.tol, "Tolerance.");
    app.add_option(
        "-r,--relative",
        args.relative,
        "Whether to use a tolerance relative to the bbox diagonal.");
    CLI11_PARSE(app, argc, argv)

    lagrange::logger().set_level(spdlog::level::trace);

    lagrange::logger().info("Loading input mesh: {}", args.input);
    auto mesh = lagrange::io::load_mesh<lagrange::TriangleMesh3D>(args.input);

    if (args.relative) {
        double diag = mesh_bbox(*mesh).diagonal().norm();
        lagrange::logger().info(
            "Using a relative tolerance of {:.3f} x {:.3f} = {:.3f}",
            args.tol,
            diag,
            args.tol * diag);
        args.tol *= diag;
    }

    if (args.max_holes) {
        lagrange::logger().info("Closing small holes");
        mesh = lagrange::close_small_holes(*mesh, args.max_holes);
    }

    if (!args.holes_only) {
        lagrange::logger().info("Removing degenerate triangles");
        mesh = lagrange::remove_degenerate_triangles(*mesh);
        lagrange::logger().info("Removing duplicate vertices");
        mesh = lagrange::remove_duplicate_vertices(*mesh);
        lagrange::logger().info("Splitting long edges");
        mesh = lagrange::split_long_edges(*mesh, args.tol * args.tol, true);
    }

    lagrange::logger().info("Saving result: {}", args.output);
    lagrange::io::save_mesh(args.output, *mesh);

    return 0;
}

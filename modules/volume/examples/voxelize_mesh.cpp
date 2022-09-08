/*
 * Copyright 2021 Adobe. All rights reserved.
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
#include <lagrange/volume/mesh_to_volume.h>
#include <lagrange/volume/volume_to_mesh.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <igl/bounding_box_diagonal.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <CLI/CLI.hpp>

using MeshType = lagrange::TriangleMesh3D;

int main(int argc, char** argv)
{
    struct
    {
        std::string input;
        std::string output = "output.obj";
        double voxel_size = 0.001;
        double isovalue = 0.0;
        double adaptivity = 0.0;
        bool relative = true;
    } args;

    CLI::App app{argv[0]};
    app.add_option("input", args.input, "Input mesh.")->required()->check(CLI::ExistingFile);
    app.add_option("output", args.output, "Output mesh.");
    app.add_option("-s,--voxel-size", args.voxel_size, "Voxel size.");
    app.add_option("-v,--isovalue", args.isovalue, "Isovalue to mesh.");
    app.add_option("-a,--adaptivity", args.adaptivity, "Mesh adaptivity between [0, 1].");
    app.add_option(
        "-r,--relative",
        args.relative,
        "Whether to use a voxel size relative to the bbox diagonal.");
    CLI11_PARSE(app, argc, argv)

    lagrange::logger().info("Loading input mesh: {}", args.input);
    auto mesh = lagrange::io::load_mesh<MeshType>(args.input);

    if (args.relative) {
        double diag = igl::bounding_box_diagonal(mesh->get_vertices());
        lagrange::logger().info(
            "Using a relative voxel size of {:.3f} x {:.3f} = {:.3f}",
            args.voxel_size,
            diag,
            args.voxel_size * diag);
        args.voxel_size *= diag;
    }

    lagrange::logger().info("Mesh to volume conversion");
    auto grid = lagrange::volume::mesh_to_volume(*mesh, args.voxel_size);

    lagrange::logger().info("Volume to mesh conversion");
    mesh = lagrange::volume::volume_to_mesh<MeshType>(*grid, args.isovalue, args.adaptivity);

    lagrange::logger().info("Saving result: {}", args.output);
    lagrange::io::save_mesh(args.output, *mesh);

    return 0;
}

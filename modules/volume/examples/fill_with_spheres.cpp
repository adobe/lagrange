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
#include <lagrange/combine_mesh_list.h>
#include <lagrange/create_mesh.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/volume/fill_with_spheres.h>
#include <lagrange/volume/mesh_to_volume.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <igl/bounding_box_diagonal.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <CLI/CLI.hpp>

using MeshType = lagrange::TriangleMesh3D;
using Scalar = double;

std::unique_ptr<MeshType> generate_sphere(double radius, const Eigen::RowVector3d& center)
{
    auto mesh = lagrange::create_sphere();
    lagrange::Vertices3D vertices;
    mesh->export_vertices(vertices);
    for (Eigen::Index i = 0; i < vertices.rows(); ++i) {
        vertices.row(i) = vertices.row(i) * radius + center;
    }
    mesh->import_vertices(vertices);
    return mesh;
}

int main(int argc, char** argv)
{
    struct
    {
        std::string input;
        std::string output = "output.obj";
        double voxel_size = 0.001;
        bool relative = true;
        int num_spheres = 50;
        bool overlap = true;
    } args;

    CLI::App app{argv[0]};
    app.add_option("input", args.input, "Input mesh.")->required()->check(CLI::ExistingFile);
    app.add_option("output", args.output, "Output mesh.");
    app.add_option("-s,--voxel-size", args.voxel_size, "Voxel size.");
    app.add_option("-n,--num-spheres", args.num_spheres, "Max number of spheres");
    app.add_flag("--overlap,!--no-overlap", args.overlap, "Allow overlaps");
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

    lagrange::logger().info("Filling with spheres");
    Eigen::Matrix<float, Eigen::Dynamic, 4, Eigen::RowMajor> spheres;
    lagrange::volume::fill_with_spheres(*grid, spheres, args.num_spheres, args.overlap);

    lagrange::logger().info("Converting to triangle mesh");
    std::vector<std::unique_ptr<MeshType>> meshes;
    for (Eigen::Index i = 0; i < spheres.rows(); ++i) {
        const Eigen::RowVector3d center = spheres.row(i).head<3>().cast<double>();
        const double radius = spheres(i, 3);
        meshes.emplace_back(generate_sphere(radius, center));
    }
    mesh = lagrange::combine_mesh_list(meshes);

    lagrange::logger().info("Saving result: {}", args.output);
    lagrange::io::save_mesh(args.output, *mesh);

    return 0;
}

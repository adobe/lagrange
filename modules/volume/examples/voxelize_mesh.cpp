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

#include <CLI/CLI.hpp>

const std::map<std::string, lagrange::volume::MeshToVolumeOptions::Sign>& signing_types()
{
    static std::map<std::string, lagrange::volume::MeshToVolumeOptions::Sign> _methods = {
        {"FloodFill", lagrange::volume::MeshToVolumeOptions::Sign::FloodFill},
        {"WindingNumber", lagrange::volume::MeshToVolumeOptions::Sign::WindingNumber},
    };
    return _methods;
}


int main(int argc, char** argv)
{
    struct
    {
        std::string input;
        std::string output = "output.obj";
    } args;

    lagrange::volume::MeshToVolumeOptions m2v_opt;
    lagrange::volume::VolumeToMeshOptions v2m_opt;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("input", args.input, "Input mesh.")->required()->check(CLI::ExistingFile);
    app.add_option("output", args.output, "Output mesh.");
    app.add_option(
        "-s,--voxel-size",
        m2v_opt.voxel_size,
        "Voxel size. Negative means relative to bbox diagonal.");
    app.add_option("-m,--method", m2v_opt.signing_method, "Grid signing method.")
        ->transform(CLI::Transformer(signing_types(), CLI::ignore_case));
    app.add_option("-v,--isovalue", v2m_opt.isovalue, "Isovalue to mesh.");
    app.add_option("-a,--adaptivity", v2m_opt.adaptivity, "Mesh adaptivity between [0, 1].");
    CLI11_PARSE(app, argc, argv)

    spdlog::set_level(spdlog::level::debug);

    lagrange::logger().info("Loading input mesh: {}", args.input);
    auto mesh = lagrange::io::load_mesh<lagrange::SurfaceMesh32f>(args.input);

    lagrange::logger().info("Mesh to volume conversion");
    auto grid = lagrange::volume::mesh_to_volume(mesh, m2v_opt);

    lagrange::logger().info("Volume to mesh conversion");
    mesh = lagrange::volume::volume_to_mesh<lagrange::SurfaceMesh32f>(*grid, v2m_opt);

    lagrange::logger().info("Saving result: {}", args.output);
    lagrange::io::save_mesh(args.output, mesh);

    return 0;
}

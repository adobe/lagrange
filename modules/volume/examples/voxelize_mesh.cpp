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

namespace fs = lagrange::fs;

using FloatGrid = lagrange::volume::Grid<float>;
using FloatGridPtr = typename lagrange::volume::Grid<float>::Ptr;

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
        fs::path input;
        fs::path output = "output.obj";
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

    auto grid = [&]() -> FloatGridPtr {
        if (args.input.extension() == ".vdb") {
            openvdb::initialize();
            openvdb::io::File file(args.input.string());
            file.open();
            auto grids_ptr = file.getGrids();
            file.close();
            la_runtime_assert(
                grids_ptr && grids_ptr->size() == 1,
                "Input vdb must contain exactly one grid.");
            return openvdb::gridPtrCast<FloatGrid>((*grids_ptr)[0]);
        } else {
            lagrange::logger().info("Loading input mesh: {}", args.input.string());
            auto mesh = lagrange::io::load_mesh<lagrange::SurfaceMesh32f>(args.input);

            lagrange::logger().info("Mesh to volume conversion");
            return lagrange::volume::mesh_to_volume(mesh, m2v_opt);
        }
    }();

    if (args.output.extension() == ".vdb") {
        lagrange::logger().info("Saving volume to: {}", args.output.string());
        openvdb::io::File file(args.output.string());
        file.setCompression(openvdb::io::COMPRESS_BLOSC);
        file.write({grid});
        file.close();
    } else {
        lagrange::logger().info("Volume to mesh conversion");
        auto mesh = lagrange::volume::volume_to_mesh<lagrange::SurfaceMesh32f>(*grid, v2m_opt);

        lagrange::logger().info("Saving result: {}", args.output.string());
        lagrange::io::save_mesh(args.output, mesh);
    }

    return 0;
}

/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/bvh/remove_interior_shells.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/volume/internal/utils.h>
#include <lagrange/volume/mesh_to_volume.h>
#include <lagrange/volume/volume_to_mesh.h>

#ifdef NANOVDB_ENABLED
// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanovdb/tools/CreateNanoGrid.h>
#include <nanovdb/io/IO.h>
#include <lagrange/utils/warnon.h>
// clang-format on
#endif

#include <CLI/CLI.hpp>

namespace fs = lagrange::fs;

using FloatGrid = lagrange::volume::Grid<float>;

const std::map<std::string, lagrange::volume::MeshToVolumeOptions::Sign>& signing_types()
{
    static std::map<std::string, lagrange::volume::MeshToVolumeOptions::Sign> _methods = {
        {"FloodFill", lagrange::volume::MeshToVolumeOptions::Sign::FloodFill},
        {"WindingNumber", lagrange::volume::MeshToVolumeOptions::Sign::WindingNumber},
        {"Unsigned", lagrange::volume::MeshToVolumeOptions::Sign::Unsigned},
    };
    return _methods;
}

int main(int argc, char** argv)
{
    struct
    {
        fs::path input;
        fs::path output = "output.ply";
        std::optional<double> offset_radius;
    } args;
    lagrange::volume::MeshToVolumeOptions m2v_opt;
    lagrange::volume::VolumeToMeshOptions v2m_opt;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("input", args.input, "Input mesh or vdb.")->required()->check(CLI::ExistingFile);
    app.add_option("output", args.output, "Output mesh, vdb or nvdb.");
    app.add_option(
        "-s,--voxel-size",
        m2v_opt.voxel_size,
        "Voxel size. Negative means relative to bbox diagonal.");
    app.add_option("-m,--method", m2v_opt.signing_method, "Grid signing method.")
        ->transform(CLI::Transformer(signing_types(), CLI::ignore_case));
    app.add_option("-o,--offset", args.offset_radius, "Level-set offset radius.");
    app.add_option("-v,--isovalue", v2m_opt.isovalue, "Isovalue to mesh.");
    app.add_option("-a,--adaptivity", v2m_opt.adaptivity, "Mesh adaptivity between [0, 1].");
    CLI11_PARSE(app, argc, argv)

    spdlog::set_level(spdlog::level::debug);

    auto grid = [&]() -> FloatGrid::Ptr {
        const auto& path = args.input;
        if (path.extension() == ".vdb") {
            lagrange::logger().info("Loading volume from OpenVDB grid: {}", path.string());
            openvdb::initialize();
            openvdb::io::File file(path.string());
            file.open();
            auto grids_ptr = file.getGrids();
            file.close();
            // Filter level set grids
            la_runtime_assert(grids_ptr);
            openvdb::GridPtrVecPtr grids_ptr_ = std::make_unique<openvdb::GridPtrVec>();
            for (auto grid_ptr : *grids_ptr) {
                if (grid_ptr->getGridClass() == openvdb::GridClass::GRID_LEVEL_SET)
                    grids_ptr_->emplace_back(grid_ptr);
            }
            // There should be a single level set grid
            la_runtime_assert(grids_ptr_->size() == 1, "Could not find a single level set grid.");
            auto grid_ptr = (*grids_ptr)[0];
            const std::string& name = grid_ptr->getName();
            lagrange::logger().info("Using grid: {}", name);
            return openvdb::gridPtrCast<FloatGrid>(grid_ptr);
        } else {
            lagrange::logger().info("Loading input mesh: {}", path.string());
            auto mesh = lagrange::io::load_mesh<lagrange::SurfaceMesh32f>(path);
            lagrange::logger().info("Mesh to volume conversion");
            auto raw_grid = lagrange::volume::mesh_to_volume(mesh, m2v_opt);
            if (m2v_opt.signing_method == lagrange::volume::MeshToVolumeOptions::Sign::Unsigned) {
                double isovalue = raw_grid->voxelSize().x() * std::sqrt(3.0);
                lagrange::volume::VolumeToMeshOptions opt;
                opt.isovalue = v2m_opt.isovalue + isovalue;
                lagrange::logger().info(
                    "Extracting offset mesh at isovalue {} to ensure watertightness",
                    isovalue);
                auto offset_mesh =
                    lagrange::volume::volume_to_mesh<lagrange::SurfaceMesh32f>(*raw_grid, opt);
                lagrange::triangulate_polygonal_facets(offset_mesh);
                lagrange::logger().info("Removing interior shells");
                offset_mesh = lagrange::bvh::remove_interior_shells(offset_mesh);
                lagrange::logger().info("Re-voxelizing offset mesh");
                m2v_opt.signing_method = lagrange::volume::MeshToVolumeOptions::Sign::FloodFill;
                return lagrange::volume::mesh_to_volume(offset_mesh, m2v_opt);
            } else if (args.offset_radius.has_value()) {
                double offset_radius = args.offset_radius.value();
                lagrange::logger().info("Applying level-set offset of {}", offset_radius);
                lagrange::volume::internal::offset_grid_in_place(*raw_grid, offset_radius, false);
                return raw_grid;
            } else {
                return raw_grid;
            }
        }
    }();

    if (args.output.extension() == ".vdb") {
        lagrange::logger().info("Saving volume to OpenVDB grid: {}", args.output.string());
        openvdb::io::File file(args.output.string());
        file.setCompression(openvdb::io::COMPRESS_BLOSC);
        file.write({grid});
        file.close();
    } else if (args.output.extension() == ".nvdb") {
#ifdef NANOVDB_ENABLED
        lagrange::logger().info("Saving volume to NanoVDB grid: {}", args.output.string());
        auto handle = nanovdb::tools::createNanoGrid(*grid);
        nanovdb::io::writeGrid(args.output.string(), handle, nanovdb::io::Codec::BLOSC);
#else
        lagrange::logger().error("NanoVDB support is not enabled in this build.");
        return 1;
#endif
    } else {
        lagrange::logger().info("Volume to mesh conversion");
        auto mesh = lagrange::volume::volume_to_mesh<lagrange::SurfaceMesh32f>(*grid, v2m_opt);
        lagrange::logger().info("Saving mesh: {}", args.output.string());
        lagrange::io::save_mesh(args.output, mesh);
    }

    return 0;
}

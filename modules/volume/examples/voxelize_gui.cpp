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
#include "register_grid.h"

#include <lagrange/Logger.h>
#include <lagrange/bvh/remove_interior_shells.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/polyscope/register_mesh.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/volume/internal/utils.h>
#include <lagrange/volume/mesh_to_volume.h>
#include <lagrange/volume/volume_to_mesh.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <polyscope/point_cloud.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <CLI/CLI.hpp>

namespace fs = lagrange::fs;

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
        bool redistance = false;
        std::optional<double> offset_radius;
        std::optional<float> dense_voxel_size;
    } args;
    lagrange::volume::MeshToVolumeOptions m2v_opt;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("input", args.input, "Input mesh or vdb.")->required()->check(CLI::ExistingFile);
    app.add_option(
        "-s,--voxel-size",
        m2v_opt.voxel_size,
        "Voxel size. Negative means relative to bbox diagonal.");
    app.add_option("-m,--method", m2v_opt.signing_method, "Grid signing method.")
        ->transform(CLI::Transformer(signing_types(), CLI::ignore_case));
    app.add_option("-o,--offset", args.offset_radius, "Level-set offset radius.");
    auto re_flag = app.add_flag(
        "-r,--redistance",
        args.redistance,
        "Show redistanced version of the input SDF.");
    app.add_option(
           "-d,--dense-voxel-size",
           args.dense_voxel_size,
           "If set, resample the grid to a dense SDF with the given voxel size (negative means "
           "relative to input voxel size).")
        ->needs(re_flag);
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
            lagrange::triangulate_polygonal_facets(mesh);
            lagrange::logger().info("Mesh to volume conversion");
            auto raw_grid = lagrange::volume::mesh_to_volume(mesh, m2v_opt);
            if (m2v_opt.signing_method == lagrange::volume::MeshToVolumeOptions::Sign::Unsigned) {
                double isovalue = raw_grid->voxelSize().x() * std::sqrt(3.0);
                lagrange::volume::VolumeToMeshOptions v2m_opt;
                v2m_opt.isovalue = isovalue;
                lagrange::logger().info(
                    "Extracting offset mesh at isovalue {} to ensure watertightness",
                    isovalue);
                auto offset_mesh =
                    lagrange::volume::volume_to_mesh<lagrange::SurfaceMesh32f>(*raw_grid, v2m_opt);
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

    polyscope::options::configureImGuiStyleCallback = []() {
        ImGui::Spectrum::StyleColorsSpectrum();
        ImGui::Spectrum::LoadFont();
    };
    polyscope::init();

    register_grid("input_grid", *grid);
    auto extracted_mesh = lagrange::volume::volume_to_mesh<lagrange::SurfaceMesh32f>(*grid);
    lagrange::polyscope::register_mesh("input_mesh", extracted_mesh);

    if (args.redistance) {
        lagrange::logger().info("Redistancing the SDF");
        if (args.dense_voxel_size.has_value()) {
            lagrange::logger().info(
                "Resampling to dense SDF with voxel size {}",
                *args.dense_voxel_size);
            grid = lagrange::volume::internal::resample_grid(*grid, *args.dense_voxel_size);
        }
        grid = lagrange::volume::internal::densify_grid(*grid);
        grid = lagrange::volume::internal::redistance_grid(*grid);
        register_grid("redistanced_grid", *grid);
        auto sdf_mesh = lagrange::volume::volume_to_mesh<lagrange::SurfaceMesh32f>(*grid);
        lagrange::polyscope::register_mesh("redistanced_mesh", sdf_mesh);
    }

    polyscope::show();

    return 0;
}

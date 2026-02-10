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

#include "register_grid.h"

#include <lagrange/fs/filesystem.h>
#include <lagrange/volume/mesh_to_volume.h>
#include <lagrange/volume/volume_to_mesh.h>

#ifdef NANOVDB_ENABLED
// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanovdb/tools/NanoToOpenVDB.h>
#include <nanovdb/io/IO.h>
#include <lagrange/utils/warnon.h>
// clang-format on
#endif

#include <CLI/CLI.hpp>

namespace fs = lagrange::fs;

using ConstFloatGridPtr = typename lagrange::volume::Grid<float>::ConstPtr;

int main(int argc, char** argv)
{
    struct
    {
        std::vector<fs::path> inputs;
    } args;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("input", args.inputs, "Input grids.")->required()->check(CLI::ExistingFile);
    CLI11_PARSE(app, argc, argv)

    spdlog::set_level(spdlog::level::debug);

    // TODO: Handle both float and double
    auto load_grid = [&](fs::path input) -> ConstFloatGridPtr {
        if (input.extension() == ".vdb") {
            openvdb::initialize();
            openvdb::io::File file(input.string());
            file.open();
            auto grids_ptr = file.getGrids();
            file.close();
            la_runtime_assert(
                grids_ptr && grids_ptr->size() == 1,
                "Input vdb must contain exactly one grid.");
            return openvdb::gridPtrCast<FloatGrid>((*grids_ptr)[0]);
        } else if (input.extension() == ".nvdb") {
#ifdef NANOVDB_ENABLED
            auto handle = nanovdb::io::readGrid(input.string());
            auto grid_ptr = nanovdb::tools::nanoToOpenVDB(handle);
            return openvdb::gridPtrCast<FloatGrid>(grid_ptr);
#else
            throw lagrange::Error("NanoVDB support is not enabled in this build.");
#endif
        } else {
            throw lagrange::Error("Unsupported grid extension: " + input.string());
        }
    };

    polyscope::options::configureImGuiStyleCallback = []() {
        ImGui::Spectrum::StyleColorsSpectrum();
        ImGui::Spectrum::LoadFont();
    };
    polyscope::init();

    for (const auto& input : args.inputs) {
        auto grid = load_grid(input);
        register_grid(input.stem().string(), *grid);
    }

    polyscope::show();

    return 0;
}

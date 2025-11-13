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
#include <lagrange/attributes/attribute_utils.h>
#include <lagrange/attributes/unify_index_buffer.h>
#include <lagrange/compute_normal.h>
#include <lagrange/compute_tangent_bitangent.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/internal/constants.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/ui/UI.h>
#include <lagrange/utils/timing.h>
#include <CLI/CLI.hpp>

namespace ui = lagrange::ui;

int main(int argc, char* argv[])
{
    struct
    {
        std::string input;
        bool corner = false;
        bool pad = false;
        bool headless = false;
    } args;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("input", args.input, "Input mesh.")->required()->check(CLI::ExistingFile);
    app.add_flag("-c,--corner", args.corner, "Compute corner tangents instead of indexed.");
    app.add_flag(
        "-p,--pad",
        args.pad,
        "Pad last coordinate with sign of the uv triangle orientation.");
    app.add_flag("--headless", args.headless, "Don't launch the viewer.");
    CLI11_PARSE(app, argc, argv)


    lagrange::logger().set_level(spdlog::level::trace);
    lagrange::logger().info("Loading mesh: {}", args.input);
    auto mesh = lagrange::io::load_mesh<lagrange::TriangleMesh3D>(args.input);

    lagrange::VerboseTimer timer;

    // Compute indexed normals, tangent and bitangent
    const double EPS = 1e-3;
    timer.tick();
    lagrange::logger().info("Computing indexed normals");
    lagrange::compute_normal(*mesh, lagrange::internal::pi * 0.5 - EPS);
    lagrange::logger().info("Computing tangent frame");
    timer.tock("compute indexed normals");

    // For sake of comparison
    timer.tick();
    lagrange::compute_vertex_normal(*mesh);
    timer.tock("compute vertex normals");


    if (args.corner) {
        timer.tick();
        lagrange::compute_corner_tangent_bitangent(*mesh, args.pad);
        timer.tock("compute corner tangents");
    } else {
        timer.tick();
        lagrange::compute_indexed_tangent_bitangent(*mesh, args.pad);
        timer.tock("compute index tangents");
        lagrange::logger().info("Transfer to corner attributes");
        lagrange::map_indexed_attribute_to_corner_attribute(*mesh, "tangent");
        lagrange::map_indexed_attribute_to_corner_attribute(*mesh, "bitangent");
    }
    lagrange::logger().info("Done");
    lagrange::map_indexed_attribute_to_corner_attribute(*mesh, "normal");

    // Also for the sake of timing unify uv and normal. Result is not used.
    if (!args.corner) {
        timer.tick();
        unify_index_buffer(*mesh, {"uv", "tangent", "bitangent"});
        timer.tock("Unify buffers");
    }

    if (args.headless) {
        return 0;
    }


    ui::Viewer viewer("Tangent Frame Viewer", 1920, 1080);

    // TODO: use optional arg for mesh name?
    auto mesh_view = ui::add_mesh(viewer, std::move(mesh), "Mesh");
    auto mesh_geometry = ui::get_meshdata_entity(viewer, mesh_view);

    const auto M =
        ui::get_mesh_bounds(ui::get_mesh_data(viewer, mesh_geometry)).get_normalization_transform();

    ui::apply_transform(viewer, mesh_view, M);

    // TODO: add ground
    // viewer.enable_ground(true);
    // viewer.get_ground().enable_grid(true).enable_axes(true).set_height(-1.0f);

    float xpos = -1.0f;
    for (auto attr : {"tangent", "bitangent", "normal"}) {
        // TODO: map 3-vec to 1-vec for colormapping
        auto e = ui::show_indexed_attribute(viewer, mesh_geometry, attr, ui::Glyph::Surface);
        ui::apply_transform(viewer, e, Eigen::Translation3f(xpos, 0, -1.0f) * M);
        xpos += 1.0f;
    }

    viewer.run();

    return 0;
}

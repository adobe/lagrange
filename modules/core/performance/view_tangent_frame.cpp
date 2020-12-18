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
#include <lagrange/io/load_mesh.h>
#include <lagrange/ui/UI.h>
#include <lagrange/utils/timing.h>
#include <CLI/CLI.hpp>

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

    auto timer = lagrange::create_verbose_timer(lagrange::logger());

    // Compute indexed normals, tangent and bitangent
    const double EPS = 1e-3;
    timer.tick();
    lagrange::logger().info("Computing indexed normals");
    lagrange::compute_normal(*mesh, M_PI * 0.5 - EPS);
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

    if(args.headless){
        return 0;
    }

    lagrange::ui::Viewer::WindowOptions wopt;
    wopt.width = 1920;
    wopt.height = 1080;
    wopt.window_title = "Tangent Frame Viewer";
    wopt.vsync = false;

    lagrange::ui::Viewer viewer(wopt);
    if (!viewer.is_initialized()) return 1;

    lagrange::ui::Scene& scene = viewer.get_scene();
    scene.add_model(lagrange::ui::ModelFactory::make(std::move(mesh)));

    viewer.enable_ground(true);
    viewer.get_ground().enable_grid(true).enable_axes(true).set_height(-1.0f);

    auto colormap = [&](const lagrange::ui::Model& /*model*/,
                        const lagrange::ui::Viz::AttribValue& value) {
        Eigen::Vector3f vec = value.head<3>().cast<float>();
        auto rgb = (vec + Eigen::Vector3f::Constant(1.0f)).normalized().eval();
        if (args.pad) {
            return lagrange::ui::Color::random((int)value(3));
        } else {
            return lagrange::ui::Color(rgb.x(), rgb.y(), rgb.z(), 1.0f);
        }
    };
    viewer.add_viz(lagrange::ui::Viz::create_attribute_colormapping(
        "Tangent",
        lagrange::ui::Viz::Attribute::CORNER,
        "tangent",
        lagrange::ui::Viz::Primitive::TRIANGLES,
        colormap));
    viewer.add_viz(lagrange::ui::Viz::create_attribute_colormapping(
        "Bitangent",
        lagrange::ui::Viz::Attribute::CORNER,
        "bitangent",
        lagrange::ui::Viz::Primitive::TRIANGLES,
        colormap));
    viewer.add_viz(lagrange::ui::Viz::create_attribute_colormapping(
        "Normal",
        lagrange::ui::Viz::Attribute::CORNER,
        "normal",
        lagrange::ui::Viz::Primitive::TRIANGLES,
        colormap));

    while (!viewer.should_close()) {
        viewer.begin_frame();
        viewer.end_frame();
    }

    return 0;
}

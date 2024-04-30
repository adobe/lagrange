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

#include <lagrange/ui/UI.h>
#include <lagrange/utils/fmt_eigen.h>
#include <CLI/CLI.hpp>

namespace ui = lagrange::ui;

int main(int argc, char** argv)
{
    struct
    {
        bool verbose = false;
    } args;

    CLI::App app{argv[0]};
    app.add_flag("-v", args.verbose, "verbose");
    CLI11_PARSE(app, argc, argv)

    if (args.verbose) {
        lagrange::logger().set_level(spdlog::level::debug);
    }


    // Initialize the viewer
    ui::Viewer viewer("UI Example - Callbacks", 1920, 1080);

    // Get reference to the registry (global state of the ui)
    const auto mesh = ui::register_mesh(viewer, lagrange::create_sphere());
    const int n = 16;
    for (auto i = 0; i < 16; i++) {
        auto instance = ui::show_mesh(viewer, mesh);
        float t = i / float(n);

        ui::set_transform(viewer, instance, Eigen::Scaling(ui::pi() / n));
        ui::apply_transform(
            viewer,
            instance,
            Eigen::Translation3f(std::sin(t * ui::two_pi()), 0, std::cos(t * ui::two_pi())));
        ui::get_material(viewer, instance)
            ->set_color(ui::PBRMaterial::BaseColor, ui::colormap_viridis(t));
        ui::set_name(viewer, instance, lagrange::string_format("instance {}", i));
    }


    /*
        See <lagrange/ui/default_events.h> for a list of event types
    */

    // System window resized
    ui::on<ui::WindowResizeEvent>(viewer, [](const ui::WindowResizeEvent& e) {
        lagrange::logger().info("Window resized to: {}x{}", e.width, e.height);
    });

    // Called just before window closes
    ui::on<ui::WindowCloseEvent>(viewer, [](const ui::WindowCloseEvent& /*e*/) {
        lagrange::logger().info("Window is closing");
    });

    // Drag and drop of files
    ui::on<ui::WindowDropEvent>(viewer, [](const ui::WindowDropEvent& e) {
        lagrange::logger().info("Dropped {} files", e.count);
        for (auto i = 0; i < e.count; i++) {
            lagrange::logger().info("\t{}", e.paths[i]);
        }
    });

    // Transform component of an entity changed
    ui::on<ui::TransformChangedEvent>(viewer, [&](const ui::TransformChangedEvent& e) {
        lagrange::logger().info(
            "Transform of entity {} changed. Position:\n{}",
            static_cast<uint64_t>(e.entity),
            ui::get_transform(viewer, e.entity).global.translation());
    });

    // Camera component of an entity changed
    ui::on<ui::CameraChangedEvent>(viewer, [&](const ui::CameraChangedEvent& e) {
        const auto& cam = ui::get_camera(viewer, e.entity);
        lagrange::logger().info(
            "Camera of entity {} changed. Position:\n{}",
            static_cast<uint64_t>(e.entity),
            cam.get_position());
    });


    // Selection and hover events
    ui::on<ui::SelectedEvent>(viewer, [](const ui::SelectedEvent& e) {
        lagrange::logger().info("Entity {} selected", static_cast<uint64_t>(e.entity));
    });
    ui::on<ui::DeselectedEvent>(viewer, [](const ui::DeselectedEvent& e) {
        lagrange::logger().info("Entity {} deselected", static_cast<uint64_t>(e.entity));
    });
    ui::on<ui::HoveredEvent>(viewer, [](const ui::HoveredEvent& e) {
        lagrange::logger().info("Entity {} hovered", static_cast<uint64_t>(e.entity));
    });
    ui::on<ui::DehoveredEvent>(viewer, [](const ui::DehoveredEvent& e) {
        lagrange::logger().info("Entity {} dehovered", static_cast<uint64_t>(e.entity));
    });


    // You can register custom events
    struct TickEvent
    {
        double t;
    };

    // First register a listener, then use ui::publish to trigger the event
    ui::on<TickEvent>(viewer, [](const TickEvent& e) {
        lagrange::logger().info("Tick, t = {}", e.t);
    });


    double t_from_last = 0;
    viewer.run([&](ui::Registry& r) {
        const auto& gtime = r.ctx().get<ui::GlobalTime>();

        // Get elapsed time from last frame, add it to variable
        t_from_last += gtime.dt;

        // Trigger event every two seconds
        if (t_from_last > 2.0) {
            ui::publish<TickEvent>(r, gtime.t);
            t_from_last = 0;
        }

        return true;
    });

    return 0;
}

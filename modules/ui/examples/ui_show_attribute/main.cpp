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

#include <lagrange/ui/UI.h>
#include <CLI/CLI.hpp>

#include <lagrange/compute_dijkstra_distance.h>
#include <lagrange/compute_edge_lengths.h>
#include <lagrange/compute_normal.h>
#include <lagrange/compute_tangent_bitangent.h>
#include <lagrange/compute_vertex_normal.h>

namespace ui = lagrange::ui;

int main(int argc, char** argv)
{
    struct
    {
        std::string input;
        bool verbose = false;
    } args;

    CLI::App app{argv[0]};
    app.add_option("input", args.input, "Input mesh.");
    app.add_flag("-v", args.verbose, "verbose");
    CLI11_PARSE(app, argc, argv)

    if (args.verbose) {
        lagrange::logger().set_level(spdlog::level::debug);
    }

    if (args.input.length() == 0) {
        lagrange::logger().error("Input mesh must be specified");
        return 1;
    }

    // Initialize the viewer
    ui::Viewer viewer("UI Example - Show Attribute", 1920, 1080);

    // Get reference to the registry (global state of the ui)
    auto& r = viewer.registry();

    // Load a mesh, returns a handle
    // This will only register the mesh, but it will not show it
    lagrange::io::MeshLoaderParams p;
    p.normalize = true;
    auto mesh_entity = ui::load_obj<lagrange::TriangleMesh3D>(r, args.input, p);

    // Retrieve the mesh
    auto& mesh = ui::get_mesh<lagrange::TriangleMesh3D>(r, mesh_entity);

    // Compute attributes to visualize
    {
        lagrange::compute_dijkstra_distance(
            mesh,
            0,
            Eigen::Vector3d(1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0));
        lagrange::compute_vertex_valence(mesh);
        lagrange::compute_vertex_normal(mesh);
        lagrange::compute_triangle_normal(mesh);
        lagrange::compute_edge_lengths(mesh);
        if (mesh.is_uv_initialized()) {
            lagrange::compute_corner_tangent_bitangent(mesh);
        }
    }


    // Show attribute in the scene
    // This will create a scene object for each of the visualized attributes with material and
    // shader setup needed for visualizing the given attribute with given glyph
    std::vector<ui::Entity> scene_entities;

    const auto glyph = ui::Glyph::Surface;

    {
        // Show dijkstra_distance vertex attribute using viridis colormap
        auto e = ui::show_vertex_attribute(r, mesh_entity, "dijkstra_distance", glyph);
        ui::set_colormap(r, e, ui::generate_colormap(ui::colormap_viridis));
        scene_entities.push_back(e);
    }


    {
        // Show vertex valence vertex attribute using magma colormap
        auto e = ui::show_vertex_attribute(r, mesh_entity, "valence", glyph);
        ui::set_colormap(r, e, ui::generate_colormap(ui::colormap_magma));
        scene_entities.push_back(e);
    }


    {
        // Show vertex normal, mapped to rgb
        auto e = ui::show_vertex_attribute(r, mesh_entity, "normal", glyph);
        scene_entities.push_back(e);
    }

    {
        // Show facet normal, mapped to rgb
        auto e = ui::show_facet_attribute(r, mesh_entity, "normal", glyph);
        scene_entities.push_back(e);
    }


    {
        // Show corner attributes tangent and bitangent
        if (mesh.is_uv_initialized()) {
            scene_entities.push_back(ui::show_corner_attribute(r, mesh_entity, "tangent", glyph));
            scene_entities.push_back(ui::show_corner_attribute(r, mesh_entity, "bitangent", glyph));
        }
    }

    {
        // Show edge lengths attribute
        auto e = ui::show_edge_attribute(r, mesh_entity, "length", glyph);
        ui::set_colormap(r, e, ui::generate_colormap(ui::colormap_turbo));
        scene_entities.push_back(e);
    }

    if (mesh.is_uv_initialized()) {
        auto e = ui::show_indexed_attribute(r, mesh_entity, "uv", glyph);
        ui::set_colormap(r, e, ui::generate_colormap(ui::colormap_coolwarm));
        scene_entities.push_back(e);
    }

    auto group = ui::group(r, scene_entities);
    ui::set_name(r, group, "Attribute visualizations");

    // Distribute all the scene entities in a grid
    const int rows = std::max(1, int(sqrtf(float(scene_entities.size()))));
    const int cols = (int(scene_entities.size()) + rows - 1) / rows;

    for (size_t i = 0; i < scene_entities.size(); i++) {
        int row = (int)i % cols;
        int col = (int)i / cols;

        r.get<ui::Transform>(scene_entities[i]).local =
            Eigen::Translation3f(float(row), 0.0f, float(col)) *
            Eigen::Translation3f(float(rows) * -0.5f, 0.0f, float(cols) * -0.5f);
    }


    viewer.run();

    return 0;
}

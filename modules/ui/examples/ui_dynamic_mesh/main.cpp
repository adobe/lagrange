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

#include <lagrange/compute_facet_area.h>
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
    ui::Viewer viewer("UI Example - Dynamic Mesh", 1920, 1080);

    // Load a mesh, returns a handle
    // This will only register the mesh, but it will not show it
    lagrange::io::MeshLoaderParams params;
    params.normalize = true;
    auto mesh_entity = ui::load_obj<lagrange::TriangleMesh3D>(viewer, args.input, params);

    // Retrieve the mesh and compute some attributes
    auto& mesh = ui::get_mesh<lagrange::TriangleMesh3D>(viewer, mesh_entity);
    {
        lagrange::compute_vertex_normal(mesh, igl::PER_VERTEX_NORMALS_WEIGHTING_TYPE_UNIFORM);
        lagrange::compute_facet_area(mesh);
    }

    // Show PBR render
    {
        auto mesh_pbr = ui::show_mesh(viewer, mesh_entity);
        ui::set_name(viewer, mesh_pbr, "Mesh PBR");
    }

    // Show vertex normal -> surface visualization
    {
        auto mesh_normal_viz =
            ui::show_vertex_attribute(viewer, mesh_entity, "normal", ui::Glyph::Surface);
        ui::set_name(viewer, mesh_normal_viz, "Vertex Normals");
        ui::set_transform(viewer, mesh_normal_viz, Eigen::Translation3f(-2.0f, 0, 0));
    }

    // Show facet area colormap visualization
    ui::Entity area_viz;
    {
        area_viz = ui::show_facet_attribute(viewer, mesh_entity, "area", ui::Glyph::Surface);
        ui::set_name(viewer, area_viz, "Facet Area");
        ui::set_transform(viewer, area_viz, Eigen::Translation3f(2.0f, 0, 0));
        ui::set_colormap(viewer, area_viz, ui::generate_colormap(ui::colormap_coolwarm));
    }

    // Copy original vertices
    auto V0 = mesh.get_vertices();
    double t = 0.0;


    viewer.run(
        // This function run at the beginning of every frame
        [&](ui::Registry& r) {
            // Modify the mesh vertices
            {
                lagrange::TriangleMesh3D::VertexArray V;
                mesh.export_vertices(V);
                t += viewer.get_frame_elapsed_time();
                double a = (std::sin(2 * t) + 1.0) * 0.5;

                for (auto i = 0; i < V.rows(); i++) {
                    V.row(i) = V0.row(i).normalized() * a + V0.row(i) * (1.0 - a);
                }
                mesh.import_vertices(V);
            }

            // Recompute normal attribute
            if (mesh.has_facet_attribute("normal")) mesh.remove_facet_attribute("normal");
            if (mesh.has_vertex_attribute("normal")) mesh.remove_vertex_attribute("normal");
            if (mesh.has_corner_attribute("normal")) mesh.remove_corner_attribute("normal");
            lagrange::compute_vertex_normal(mesh, igl::PER_VERTEX_NORMALS_WEIGHTING_TYPE_UNIFORM);

            // Recompute area attribute
            if (mesh.has_facet_attribute("area")) mesh.remove_facet_attribute("area");
            lagrange::compute_facet_area(mesh);


            // Let the UI know what to update
            ui::set_mesh_vertices_dirty(viewer, mesh_entity);
            ui::set_mesh_normals_dirty(viewer, mesh_entity);

            // Any visualization using facet "area" attribute will get updated
            ui::set_mesh_attribute_dirty(viewer, mesh_entity, ui::IndexingMode::FACE, "area");


            return true;
        });

    return 0;
}

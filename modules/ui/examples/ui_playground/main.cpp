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

#include <lagrange/ui/components/ShadowMap.h>
#include <CLI/CLI.hpp>

#include <lagrange/compute_dijkstra_distance.h>
#include <lagrange/compute_normal.h>
#include <lagrange/compute_tangent_bitangent.h>
#include <lagrange/compute_vertex_normal.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <spdlog/fmt/ostr.h>
#include <lagrange/utils/warnon.h>
// clang-format on


/*
    This is an experimental example, combining multiple features of the UI.
*/

namespace ui = lagrange::ui;

void add_subtree(
    ui::Viewer& v,
    ui::Entity geometry,
    ui::Entity parent,
    int max_depth,
    int depth = 0)
{
    if (depth == max_depth) return;

    auto& w = v.registry();

    auto sub0 = ui::show_mesh(w, geometry);
    auto sub1 = ui::show_mesh(w, geometry);


    ui::set_name(w, sub0, lagrange::string_format("{}_left", depth));
    ui::set_name(w, sub1, lagrange::string_format("{}_right", depth));


    const float scale_factor = 0.5f;
    w.get<ui::Transform>(sub0).local =
        Eigen::Translation3f(1.0f, 1.0f, 0) * Eigen::Scaling(scale_factor);
    w.get<ui::Transform>(sub1).local =
        Eigen::Translation3f(-1.0f, 1.0f, 0) * Eigen::Scaling(scale_factor);

    ui::set_parent(w, sub0, parent);
    ui::set_parent(w, sub1, parent);

    // Todo better api, e.g. ui::set_pbr_material()
    w.get<ui::MeshRender>(sub0).material->set_color(
        "material_base_color",
        ui::Color::random(int(sub0)));

    w.get<ui::MeshRender>(sub1).material->set_color(
        "material_base_color",
        ui::Color::random(int(sub1)));

    add_subtree(v, geometry, sub0, max_depth, depth + 1);
    add_subtree(v, geometry, sub1, max_depth, depth + 1);
}


int main(int argc, char** argv)
{
    /*
        Set up initial window and OpenGL context options
    */

    struct
    {
        std::string input;
    } args;

    CLI::App app{argv[0]};
    app.add_option("input", args.input, "Input mesh.");
    CLI11_PARSE(app, argc, argv)


    ui::Viewer::WindowOptions wopt;
    wopt.width = 1920;
    wopt.height = 1080;
    wopt.window_title = "UI Playground Example";
    wopt.vsync = false;


    /*
        Initialize the viewer
    */
    ui::Viewer viewer(wopt);

    /*
        Check if everything initialized
    */
    if (!viewer.is_initialized()) return 1;


    // Test shaders
    {
        auto& shaders = ui::get_registered_shaders(viewer);
        for (auto& it : shaders) {
            // This will invoke shader compilation
            auto tmp_mat = ui::create_material(viewer, it.first);
        }
    }


    // Test mesh types and meta
    {
        ui::MeshData d;
        auto cube_double = lagrange::create_cube();
        auto cube_float = lagrange::create_mesh(
            cube_double->get_vertices().cast<float>(),
            cube_double->get_facets());

        lagrange::compute_vertex_valence(*cube_double);
        lagrange::compute_vertex_valence(*cube_float);

        d.mesh = std::move(cube_double);
        d.type = entt::type_id<lagrange::TriangleMesh3D>();

        auto& true_ref = reinterpret_cast<const lagrange::TriangleMesh3D&>(*d.mesh);
        lagrange::logger().info("V0:\n{}", true_ref.get_vertices());

        auto vertices = ui::get_mesh_vertices(d);
        auto facets = ui::get_mesh_facets(d);

        lagrange::logger().info("V:\n{}", vertices);
        lagrange::logger().info("F:\n{}", facets);

        auto valence = ui::get_mesh_vertex_attribute(d, "valence");
        lagrange::logger().info("valence:\n{}", valence);

        Eigen::MatrixXf m;
        m.cast<float>();
        char k;
        k = 0;
    }


    lagrange::logger().set_level(spdlog::level::debug);
    auto& registry = viewer.registry();


    // Creates a mesh entity
    ui::Entity my_mesh;

    if (args.input.length() > 0) {
        lagrange::io::MeshLoaderParams p;
        p.normalize = true;
        my_mesh = ui::load_obj<lagrange::TriangleMesh3Df>(registry, args.input, p);
    } else {
        lagrange::Vertices3Df vertices = lagrange::create_cube()->get_vertices().cast<float>();
        lagrange::Triangles facets = lagrange::create_cube()->get_facets();
        my_mesh = ui::register_mesh(registry, lagrange::create_mesh(vertices, facets));
    }

    {
        auto* basemesh = registry.get<ui::MeshData>(my_mesh).mesh.get();
        auto* mesh = dynamic_cast<lagrange::TriangleMesh3Df*>(basemesh);
        assert(mesh);
        if (mesh) {
            lagrange::compute_dijkstra_distance(*mesh, 0, Eigen::Vector3f(0.3f, 0.3f, 0.3f));
            lagrange::compute_vertex_valence(*mesh);
            lagrange::compute_vertex_normal(*mesh);
            lagrange::compute_triangle_normal(*mesh);
            if (!mesh->is_uv_initialized()) {
                lagrange::logger().info("Creating trivial uvs");
                mesh->import_uv(
                    mesh->get_vertices().leftCols(2),
                    lagrange::TriangleMesh3Df::UVIndices(mesh->get_facets()));
            }
            lagrange::compute_corner_tangent_bitangent(*mesh);

            using AttrArray = lagrange::TriangleMesh3Df::AttributeArray;

            mesh->add_vertex_attribute("bone_ids");
            AttrArray bone_ids = AttrArray(mesh->get_num_vertices(), 4);

            for (auto i = 0; i < bone_ids.rows(); i++) {
                bone_ids(i, 0) = (i < bone_ids.rows() / 2) ? 0.0f : 1.0f;
                bone_ids(i, 1) = 0.0f;
                bone_ids(i, 2) = 0.0f;
                bone_ids(i, 3) = 0.0f;
            }
            mesh->import_vertex_attribute("bone_ids", std::move(bone_ids));

            mesh->add_vertex_attribute("bone_weights");
            AttrArray bone_weights = AttrArray(mesh->get_num_vertices(), 4);

            for (auto i = 0; i < bone_weights.rows(); i++) {
                bone_weights(i, 0) = 1.0;
                bone_weights(i, 1) = 0;
                bone_weights(i, 2) = 0;
                bone_weights(i, 3) = 0;
            }
            mesh->import_vertex_attribute("bone_weights", std::move(bone_weights));
        }
    }

    {
        auto dijkstra =
            ui::show_vertex_attribute(registry, my_mesh, "dijkstra_distance", ui::Glyph::Surface);
        ui::set_transform(registry, dijkstra, Eigen::Translation3f(0, 0, -2.0f));

        auto viridis = ui::generate_colormap(ui::colormap_viridis);
        ui::set_colormap(registry, dijkstra, viridis);
    }

    {
        auto valence = ui::show_vertex_attribute(registry, my_mesh, "valence", ui::Glyph::Surface);
        registry.get<ui::Transform>(valence).local = Eigen::Translation3f(-1.0f, 0, -2.0f);
    }

    {
        auto normal = ui::show_vertex_attribute(registry, my_mesh, "normal", ui::Glyph::Surface);
        registry.get<ui::Transform>(normal).local = Eigen::Translation3f(1.0f, 0, -2.0f);
    }

    {
        auto fnormal = ui::show_facet_attribute(registry, my_mesh, "normal", ui::Glyph::Surface);
        registry.get<ui::Transform>(fnormal).local = Eigen::Translation3f(1.0f, 1.0f, -2.0f);
    }

    {
        auto attrib_render =
            ui::show_corner_attribute(registry, my_mesh, "tangent", ui::Glyph::Surface);
        registry.get<ui::Transform>(attrib_render).local = Eigen::Translation3f(-2.0f, 1.0f, -2.0f);
    }

    {
        auto attrib_render =
            ui::show_corner_attribute(registry, my_mesh, "bitangent", ui::Glyph::Surface);
        registry.get<ui::Transform>(attrib_render).local =
            Eigen::Translation3f(-2.0f, -1.0f, -2.0f);
    }


    // Creates a default visualization (PBR) of the mesh entity
    auto obj_pbr =
        ui::show_mesh(registry, my_mesh, ui::DefaultShaders::PBR, {{"SKELETAL", {"On"}}});


    registry.emplace_or_replace<ui::Name>(obj_pbr, "root");
    add_subtree(viewer, my_mesh, obj_pbr, 4);


    // Window
    struct MyPanelState
    {
        int x = 42;
        int y = 7;
    };


    /*
     * Register new window type, set behavior of the window
     */
    auto panel_fn = [](ui::Registry& registry, ui::Entity e) {
        auto& s = registry.get<MyPanelState>(e);
        ImGui::Text("Local panel state:");
        ImGui::InputInt("x", &s.x);
        ImGui::InputInt("y", &s.y);

        ImGui::Text("Shared state from other system:");
        auto pos = ui::get_input(registry).mouse.position;
        ImGui::InputFloat2("Mouse pos:", pos.data());

        ImGui::Text("Shared state created and modified by these panels");


        struct MyPrivateContextVar
        {
            float x;

            ui::Entity viz_e = ui::NullEntity;
        };
        auto& priv = registry.ctx_or_set<MyPrivateContextVar>(MyPrivateContextVar{16.0f});

        ImGui::InputFloat("MyPrivateContextVar.x:", &priv.x);

        if (priv.viz_e != ui::NullEntity) {
            ui::show_widget(registry, priv.viz_e, entt::resolve(entt::type_id<ui::Transform>()));
        }
    };

    // Create panel instances with different data
    auto w1 = ui::add_panel(registry, "Mypanel", panel_fn);
    registry.emplace<MyPanelState>(w1);
    auto w2 = ui::add_panel(registry, "Mypanel 2", panel_fn);
    registry.emplace<MyPanelState>(w2, MyPanelState{0, 0});

    struct MyRotatingComponent
    {
        float speed = 1.0f;
        Eigen::Vector3f axis = Eigen::Vector3f::UnitY();
    };

    ui::register_component<MyRotatingComponent>("My Rotating Component");
    ui::register_component_widget<MyRotatingComponent>([](ui::Registry* w, ui::Entity e) {
        auto& rot = w->get<MyRotatingComponent>(e);
        if (ImGui::DragFloat3("Axis", rot.axis.data())) {
            rot.axis.normalize();
        }
        ImGui::DragFloat("Speed", &rot.speed);
    });


    viewer.systems().add(ui::Systems::Stage::Interface, [](ui::Registry& r) {
        auto dt = float(r.ctx<ui::GlobalTime>().dt);
        auto view = r.view<MyRotatingComponent, ui::Transform>();

        for (auto e : view) {
            auto& transform = view.get<ui::Transform>(e);
            auto& rot = view.get<MyRotatingComponent>(e);
            transform.local = transform.local * Eigen::AngleAxisf(rot.speed * dt, rot.axis);
        }
    });

    ui::add_point_light(registry, 30.0f * Eigen::Vector3f(0, 0, 1), Eigen::Vector3f(-1, 1, -1));
    ui::add_directional_light(registry, Eigen::Vector3f(0, 0.5f, 0), Eigen::Vector3f(1, -1, 1));
    ui::add_spot_light(
        registry,
        30.0f * Eigen::Vector3f(1, 0, 0),
        Eigen::Vector3f(1, 1, 1),
        Eigen::Vector3f(-1, -1, -1));

    auto ground_plane =
        ui::show_mesh(registry, ui::register_mesh(registry, lagrange::create_quad(false)));


    ui::set_transform(
        registry,
        ground_plane,
        Eigen::Scaling(10.0f, 1.0f, 10.0f) * Eigen::Translation3f(0.0f, -1.0f, 0.0f) *
            Eigen::AngleAxisf(-ui::pi() / 2.0f, Eigen::Vector3f::UnitX()));


    float t = 0;

    viewer.run([&](ui::Registry& registry) {
        t += float(viewer.get_frame_elapsed_time());


        // Update bones of a mesh
        Eigen::Affine3f a, b;
        a = Eigen::Translation3f(0, std::sin(t), 0);
        b = Eigen::Translation3f(std::sin(t), 0, 0);
        Eigen::Matrix4f bones[2] = {a.matrix(), b.matrix()};
        if (registry.valid(obj_pbr)) {
            ui::get_material(registry, obj_pbr)->set_mat4_array("bones", bones, 2);
        }

        return true;
    });

    return 0;
}

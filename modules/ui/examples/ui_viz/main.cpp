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
#include <imgui.h>
#include <lagrange/Logger.h>
#include <lagrange/ui/UI.h>

#include <array>
#include "VizBuilder.h"
#include "initialize_attributes.h"


using namespace lagrange::ui;


int main()
{
    Viewer::WindowOptions wopt;
    wopt.width = 1920;
    wopt.height = 1080;
    wopt.window_title = "Example Viz";

    Viewer viewer(wopt);

    if (!viewer.is_initialized()) return 1;
    Scene& scene = viewer.get_scene();

    scene.add_callback<Scene::OnModelAdd>([](Model& model) {
        model.visit_tuple<SupportedMeshTypes3DTriangle>(initialize_attributes_visitor());
        model.visit_tuple<SupportedMeshTypes3DQuad>([](auto&) {
            lagrange::logger().error(
                "This demo works only for triangle meshes. This mesh has quads");
        });

        model.add_callback<Model::OnChange>([](Model& model2) {
            model2.visit_tuple<SupportedMeshTypes3DTriangle>(initialize_attributes_visitor());
            model2.visit_tuple<SupportedMeshTypes3DQuad>([](auto&) {
                lagrange::logger().error(
                    "This demo works only for triangle meshes. This mesh has quads");
            });
        });
    });


    auto lagrange_mesh = lagrange::to_shared_ptr(lagrange::create_sphere(4));

    scene.add_model(ModelFactory::make(lagrange_mesh));


    /*
        Rendering is performed through a series of render passes
        Render passes can be added to the Renderer

        Rendering is performed for every Viewport.
        Each Viewport can enable/disable different render passes.
    */


    /*
        The Viz can help build render passes with simplified API
        1) use existing functions such as Viz::uniform_color or Viz::indexed_colormapping
        2) Create your own Viz::Config and pass it to viewer.add_viz();
    */

    Renderer& renderer = viewer.get_renderer();


    /*
        Will render mesh points in a uniform color
    */
    auto uniform = viewer.add_viz(
        Viz::create_uniform_color("Uniform Color example", Viz::Primitive::POINTS, Color::white()), false);
    uniform->add_tag("custom");


    /*
        Use indexed colormapping to assign colors using ATTRIBUTE indices.
        ATTRIBUTE is either VERTEX, EDGE, FACET or CORNER (mirroring lagrange::Mesh attributes)
        PRIMITIVE is the final rendered primitive
    */
    auto indexed = viewer.add_viz(
        Viz::create_indexed_colormapping("Indexed Colormapping example",
            Viz::Attribute::VERTEX,
            Viz::Primitive::TRIANGLES,
            [](const Model& model, int index) {
                Color color;
                /*
                    The model can have a lagrange mesh or arbitrary type,
                    to access it, either use model.get_mesh<MeshType> if you know the type
                    or use the following visitor function
                */
                model.visit_mesh([&](const auto& mesh) {
                    auto pos = mesh.get_vertices().row(index).transpose().eval();
                    auto bounds = model.get_bounds();

                    color = Color(bounds.normalize_point(pos), 1);
                });

                return color;
            }),
        false);
    indexed->add_tag("custom");

    /*
        Use attribute colormapping to assign colors based on ATTRIBUTE value
        In this case, it will try to get mesh->get_vertex_attribute("random_vertex_attribute") and
        call the given AttribColorFunc for every row.
        Viz::AttribValue is a dynamic row vector
    */
    auto attribute = viewer.add_viz(Viz::create_attribute_colormapping("Attribute Colormapping example",
                                        Viz::Attribute::VERTEX,
                                        "random_vertex_attribute",
                                        Viz::Primitive::LINES,
                                        [](const Model&, const Viz::AttribValue& value) {
                                            return Color(float(value.x()), float(value.y()), float(value.z()), 1.0f);
                                        }),
        false);
    attribute->add_tag("custom");


    VizBuilder viz_builder;

    while (!viewer.should_close()) {
        viewer.begin_frame();

        auto& viewport = viewer.get_focused_viewport_ui().get_viewport();

        ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Once);
        ImGui::Begin("Viz example");

        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Custom Render Passes")) {
            // List all passes
            auto& passes = renderer.get_pipeline().get_passes();
            for (auto& pass : passes) {
                if (pass->has_tag("default")) continue; // Skip system default passes

                ImGui::PushID(pass.get());
                bool enabled = viewport.is_render_pass_enabled(pass.get());
                if (ImGui::Checkbox(pass->get_name().c_str(), &enabled)) {
                    viewport.enable_render_pass(pass.get(), enabled);
                }


                ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 5);
                if (ImGui::Button(ICON_FA_TRASH_ALT)) {
                    renderer.get_pipeline().remove(pass.get());
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }

            if (ImGui::Button(
                    "Toggle Custom Passes", ImVec2(ImGui::GetContentRegionAvail().x / 2.f, 40.f))) {
                viewport.enable_render_pass_tag(
                    "custom", !viewport.is_render_pass_enabled_tag("custom"));
            }
            ImGui::SameLine();
            if (ImGui::Button(
                    "Toggle Default PBR", ImVec2(ImGui::GetContentRegionAvail().x, 40.f))) {
                viewport.enable_render_pass_tag("pbr", !viewport.is_render_pass_enabled_tag("pbr"));
            }

            ImGui::TreePop();
        }


        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Create your own")) {
            viz_builder(viewer);
            ImGui::TreePop();
        }


        ImGui::End();


        viewer.end_frame();
    }

    return 0;
}

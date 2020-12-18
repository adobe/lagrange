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
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <lagrange/ui/DetailUI.h>
#include <lagrange/ui/Emitter.h>
#include <lagrange/ui/IBL.h>
#include <lagrange/ui/Light.h>
#include <lagrange/ui/Material.h>
#include <lagrange/ui/MeshModel.h>
#include <lagrange/ui/Model.h>
#include <lagrange/ui/UIWidget.h>
#include <lagrange/ui/Viewer.h>
#include <lagrange/ui/ViewportUI.h>
#include <lagrange/ui/SelectionUI.h>
#include <lagrange/utils/strings.h>



namespace lagrange {
namespace ui {

void DetailUI::draw()
{
    this->begin();
    auto& sel = get_viewer()->get_selection().get_global().get_persistent();

    for (auto object : sel.get_selection()) {
        auto model = dynamic_cast<Model*>(object);
        if (model) {
            draw(*model);
            continue;
        }

        auto material = dynamic_cast<Material*>(object);
        if (material) {
            draw(*material);
            continue;
        }

        auto emitter = dynamic_cast<Emitter*>(object);
        if (emitter) {
            draw(*emitter);
            continue;
        }
    }


    auto& keys = get_viewer()->get_keybinds();

    ImGui::Text("Select objects/elements: ");
    ImGui::SameLine();
    ImGui::TextColored(ImColor(ImGui::Spectrum::GRAY600).Value,
        "\t%s",
        keys.to_string("viewport.camera.select", 2).c_str());

    {
        ImGui::Text("\tAdd: ");
        ImGui::SameLine();
        ImGui::TextColored(ImColor(ImGui::Spectrum::GRAY600).Value,
            "\t\t%s",
            keys.to_string("viewport.selection.select.add").c_str());
        ImGui::Text("\tSubtract: ");
        ImGui::SameLine();
        ImGui::TextColored(ImColor(ImGui::Spectrum::GRAY600).Value,
            "\t\t%s",
            keys.to_string("viewport.selection.select.erase").c_str());
    }

    ImGui::Text("Pan Camera ");
    ImGui::TextColored(ImColor(ImGui::Spectrum::GRAY600).Value,
        "\t%s",
        keys.to_string("viewport.camera.pan", 2).c_str());

    ImGui::Text("Rotate camera: ");
    ImGui::TextColored(ImColor(ImGui::Spectrum::GRAY600).Value,
        "\t%s",
        keys.to_string("viewport.camera.rotate", 2).c_str());

    ImGui::Text("Dolly camera: ");
    ImGui::TextColored(ImColor(ImGui::Spectrum::GRAY600).Value, "\t%s", "Mouse Wheel");
    ImGui::TextColored(ImColor(ImGui::Spectrum::GRAY600).Value,
        "\t%s",
        keys.to_string("viewport.camera.dolly", 2).c_str());

    this->end();
}

void DetailUI::draw(Material& mat, int index /*= -1*/)
{
    std::string label;
    if (index == -1) {
        label = string_format("{}", mat.get_name());
    } else {
        label = string_format("[MatID: {}] {}", index, mat.get_name());
    }

    ImGui::PushID(index);

    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode(label.c_str(), "%s", label.c_str())) {
        bool changed = false;
        changed |= UIWidget("baseColor")(mat, 3);
        changed |= UIWidget("roughness")(mat, 1);
        changed |= UIWidget("metallic")(mat, 1);
        changed |= UIWidget("normal")(mat, 3);
        changed |= UIWidget("height")(mat, 1, true);
        changed |= UIWidget("heightScale")(mat, 1, true);
        changed |= UIWidget("interiorColor")(mat, 3, true);
        changed |= UIWidget("glow")(mat, 1, true);
        changed |= UIWidget("opacity")(mat, 1);
        changed |= UIWidget("translucence")(mat, 1, true);
        changed |= UIWidget("indexOfRefraction")(mat, 1, true);
        changed |= UIWidget("density")(mat, 1, true);
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void DetailUI::draw(Emitter& emitter)
{
    std::string label;
    switch (emitter.get_type()) {
    case Emitter::Type::POINT: label = "Point light"; break;
    case Emitter::Type::SPOT: label = "Spot light"; break;
    case Emitter::Type::DIRECTIONAL: label = "Directional"; break;
    case Emitter::Type::IBL: label = reinterpret_cast<IBL&>(emitter).get_name() + " (IBL)"; break;
    default: label = "Unknown type"; break;
    }

    if (!ImGui::CollapsingHeader(("Emitter: " + label).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        return;

    auto type = emitter.get_type();
    auto& renderer = get_viewer()->get_renderer();

    ImGui::PushID(&emitter);

    Eigen::Vector3f intensity = emitter.get_intensity();
    if (ImGui::DragFloat3("Intensity", intensity.data(), 0.1f, 0.0f, 10000.0f)) {
        emitter.set_intensity(intensity);
    }

    if (type == Emitter::Type::POINT) {
        auto& point = reinterpret_cast<PointLight&>(emitter);

        Eigen::Vector3f pos = point.get_position();
        if (ImGui::DragFloat3("Position", pos.data(), 0.01f, -1000.0, 1000.0f)) {
            point.set_position(pos);
        }

        {
            renderer.draw_sphere_lines_simple(pos, 0.25f, Color::red());
        }
    } else if (type == Emitter::Type::SPOT) {
        auto& spot = reinterpret_cast<SpotLight&>(emitter);

        Eigen::Vector3f pos = spot.get_position();
        if (ImGui::DragFloat3("Position", pos.data(), 0.01f, -1000.0, 1000.0f)) {
            spot.set_position(pos);
        }

        Eigen::Vector3f dir = spot.get_direction();
        if (ImGui::DragFloat3("Direction", dir.data(), 0.01f, -1000.0, 1000.0f)) {
            spot.set_direction(dir.normalized());
        }

        float cone = spot.get_cone_angle();
        if (ImGui::SliderAngle("Cone", &cone, 0.0f, 90.0f)) {
            spot.set_cone_angle(cone);
        }

        {
            float length = 4.0f;
            renderer.draw_point(pos, Color::red());
            renderer.draw_line(pos, pos + dir * length, Color::green());

            renderer.draw_cone_lines(
                pos, (pos + dir * length), 0.001f, length * std::sin(cone), Color::blue(), 16);
        }
    } else if (type == Emitter::Type::DIRECTIONAL) {
        auto& directional = reinterpret_cast<DirectionalLight&>(emitter);

        Eigen::Vector3f dir = directional.get_direction();
        if (ImGui::DragFloat3("Direction", dir.data(), 0.01f, -1000.0, 1000.0f)) {
            directional.set_direction(dir.normalized());
        }

        {
            auto& cam = get_viewer()->get_current_camera();
            Eigen::Vector3f lookat = cam.get_lookat();
            renderer.draw_arrow(lookat - dir * cam.get_far() / 2.0f, lookat, Color::green(), 0.1f);
        }
    } else if (type == Emitter::Type::IBL) {
        auto& ibl = reinterpret_cast<IBL&>(emitter);

        int size = static_cast<int>(ImGui::GetContentRegionAvail().x - 10.0f);

        UIWidget("background")(ibl.get_background_rect(), size, size);
    }


    float multiply = 1.0f;
    if (ImGui::DragFloat("Multiply Intensity", &multiply, 0.1f, 0.0f, 10.0f)) {
        emitter.set_intensity((emitter.get_intensity() * multiply).cwiseMax(Eigen::Vector3f::Constant(0.0001f)));
    }

    ImGui::PopID();
}

void DetailUI::draw(Model& model)
{
    if (!ImGui::CollapsingHeader(
            ("Model: " + model.get_name()).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::PushID(&model);

    model.visit_tuple<SupportedMeshTypes>([&](auto& model_in) {
        using ModelType = std::remove_cv_t<std::remove_reference_t<decltype(model_in)>>;
        using MeshType = typename ModelType::MeshType;
        using _V = typename MeshType::VertexArray;
        using _F = typename MeshType::FacetArray;
        using Scalar = typename MeshType::Scalar;
        using Index = typename MeshType::Index;


        auto& model_specific = reinterpret_cast<ModelType&>(model);

        
        if (!model_specific.has_mesh()) {
            ImGui::TextColored({1, 0, 0, 1}, "Mesh has been exported");
            return;
        }

        auto& mesh = model_specific.get_mesh();


        auto& current_selection = (model.get_selection().get_transient().size() > 0)
                                      ? model.get_selection().get_transient()
                                      : model.get_selection().get_persistent();

        auto& V = mesh.get_vertices();
        auto& F = mesh.get_facets();

        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Type info")) {
            ImGui::Text(
                "Vertex type:\t%s", utils::type_string<typename _V::Scalar>::value().c_str());
            ImGui::Text("Dimension: \t%ldD", V.cols());

            ImGui::Text(
                "Index type:\t%s", utils::type_string<typename _F::Scalar>::value().c_str());
            if (F.cols() == 3) {
                ImGui::Text("Facets: Triangles");
            } else if (F.cols() == 4) {
                ImGui::Text("Facets: Quads/Triangles");
            } else {
                ImGui::Text("Facets: Polygon (max %ld)", F.cols());
            }
            ImGui::TreePop();
        }
        ImGui::Separator();

        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Size")) {
            ImGui::Text("Vertices:  %d", mesh.get_num_vertices());
            ImGui::Text("Faces:  %d", mesh.get_num_facets());
            if (mesh.is_edge_data_initialized()) {
                ImGui::Text("Edges:  %d", mesh.get_num_edges());
            } else {
                ImGui::TextColored({1, 0, 0, 1}, "Edge data not initialized");
                /*if (ImGui::Button("Initialize")) {
                    try {
                        mesh.initialize_edge_data();

                    }
                    catch (const std::exception& ex) {
                        logger().error("Failed to initialize edge data: {}", ex.what());
                    }
                }*/
            }
            ImGui::TreePop();
        }
        ImGui::Separator();


        if (ImGui::TreeNode("Vertices")) {
            auto& pag = m_paginated_matrices[&V];
            int i = 0, j = 0;
            Scalar new_val = 0;

            bool show_selection =
                model.get_selection().get_type() == SelectionElementType::VERTEX &&
                current_selection.size() > 0;

            if (show_selection ? pag(V, current_selection.get_selection(), i, j, new_val)
                               : pag(V, i, j, new_val)) {
                _V temp;
                auto mesh_ptr = model_specific.export_mesh();
                mesh_ptr->export_vertices(temp);
                temp(i, j) = new_val;
                mesh_ptr->import_vertices(temp);
                model_specific.import_mesh(mesh_ptr);
            }
            ImGui::TreePop();
        }
        ImGui::Separator();

        if (ImGui::TreeNode("Facets")) {
            auto& pag = m_paginated_matrices[&F];
            int i = 0, j = 0;
            Index new_val = 0;

            bool show_selection = model.get_selection().get_type() == SelectionElementType::FACE &&
                                  current_selection.size() > 0;

            if (show_selection ? pag(F, current_selection.get_selection(), i, j, new_val)
                               : pag(F, i, j, new_val)) {
                _F temp;
                auto mesh_ptr = model_specific.export_mesh();
                mesh_ptr->export_facets(temp);
                temp(i, j) = new_val;
                mesh_ptr->import_facets(temp);
                model_specific.import_mesh(mesh_ptr);
            }
            ImGui::TreePop();
        }
        ImGui::Separator();


        using Att = typename MeshType::AttributeArray;
        using AttScalar = typename Att::Scalar;

        if (ImGui::TreeNode("Vertex Attributes")) {
            ImGui::PushID("vertex");
            auto names = mesh.get_vertex_attribute_names();
            if (names.size() == 0) {
                ImGui::Text("None");
            }
            for (auto& name : names) {
                if (!ImGui::TreeNode(name.c_str())) continue;
                auto& att = mesh.get_vertex_attribute(name);
                auto& pag = m_paginated_matrices[&att];
                int i, j;
                AttScalar new_val;

                bool show_selection =
                    model.get_selection().get_type() == SelectionElementType::VERTEX &&
                    current_selection.size() > 0;

                if (show_selection ? pag(att, current_selection.get_selection(), i, j, new_val)
                                   : pag(att, i, j, new_val)) {
                    Att temp;
                    auto mesh_ptr = model_specific.export_mesh();
                    mesh_ptr->export_vertex_attribute(name, temp);
                    temp(i, j) = new_val;
                    mesh_ptr->import_vertex_attribute(name, temp);
                    model_specific.import_mesh(mesh_ptr);
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
            ImGui::TreePop();
        }
        ImGui::Separator();

        if (ImGui::TreeNode("Facet Attributes")) {
            ImGui::PushID("facet");
            auto names = mesh.get_facet_attribute_names();
            if (names.size() == 0) {
                ImGui::Text("None");
            }
            for (auto& name : names) {
                if (!ImGui::TreeNode(name.c_str())) continue;
                auto& att = mesh.get_facet_attribute(name);
                auto& pag = m_paginated_matrices[&att];
                int i, j;
                AttScalar new_val;

                bool show_selection =
                    model.get_selection().get_type() == SelectionElementType::FACE &&
                    current_selection.size() > 0;

                if (show_selection ? pag(att, current_selection.get_selection(), i, j, new_val)
                                   : pag(att, i, j, new_val)) {
                    Att temp;
                    auto mesh_ptr = model_specific.export_mesh();
                    mesh_ptr->export_facet_attribute(name, temp);
                    temp(i, j) = new_val;
                    mesh_ptr->import_facet_attribute(name, temp);
                    model_specific.import_mesh(mesh_ptr);
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
            ImGui::TreePop();
        }
        ImGui::Separator();

        if (ImGui::TreeNode("Corner Attributes")) {
            ImGui::PushID("corner");
            auto names = mesh.get_corner_attribute_names();
            if (names.size() == 0) {
                ImGui::Text("None");
            }
            for (auto& name : names) {
                if (!ImGui::TreeNode(name.c_str())) continue;
                auto& att = mesh.get_corner_attribute(name);
                auto& pag = m_paginated_matrices[&att];
                int i, j;
                AttScalar new_val;

                bool show_selection =
                    model.get_selection().get_type() == SelectionElementType::VERTEX &&
                    current_selection.size() > 0;

                if (show_selection ? pag(att, current_selection.get_selection(), i, j, new_val)
                                   : pag(att, i, j, new_val)) {
                    Att temp;
                    auto mesh_ptr = model_specific.export_mesh();
                    mesh_ptr->export_corner_attribute(name, temp);
                    temp(i, j) = new_val;
                    mesh_ptr->import_corner_attribute(name, temp);
                    model_specific.import_mesh(mesh_ptr);
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
            ImGui::TreePop();
        }
        ImGui::Separator();

        if (mesh.is_edge_data_initialized() && ImGui::TreeNode("Edge Attributes")) {
            ImGui::PushID("edge");
            auto names = mesh.get_edge_attribute_names();
            if (names.size() == 0) {
                ImGui::Text("None");
            }
            for (auto& name : names) {
                if (!ImGui::TreeNode(name.c_str())) continue;
                auto& att = mesh.get_edge_attribute(name);
                auto& pag = m_paginated_matrices[&att];
                int i, j;
                AttScalar new_val;
                bool show_selection =
                    model.get_selection().get_type() == SelectionElementType::EDGE &&
                    current_selection.size() > 0;

                if (show_selection ? pag(att, current_selection.get_selection(), i, j, new_val)
                                   : pag(att, i, j, new_val)) {
                    Att temp;
                    auto mesh_ptr = model_specific.export_mesh();
                    mesh_ptr->export_edge_attribute(name, temp);
                    temp(i, j) = new_val;
                    mesh_ptr->import_edge_attribute(name, temp);
                    model_specific.import_mesh(mesh_ptr);
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
            ImGui::TreePop();
        }
    });


    // Materials
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Materials")) {
        for (auto& it : model.get_materials()) {
            draw(*it.second, it.first);
        }
        ImGui::TreePop();
    }
    ImGui::Separator();

    ImGui::PopID();
}


} // namespace ui
} // namespace lagrange

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
#include <lagrange/Mesh.h>
#include <lagrange/fs/file_utils.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/ui/GLContext.h>
#include <lagrange/ui/IBL.h>
#include <lagrange/ui/Light.h>
#include <lagrange/ui/Material.h>
#include <lagrange/ui/MeshModel.h>
#include <lagrange/ui/Model.h>
#include <lagrange/ui/ModelFactory.h>
#include <lagrange/ui/Renderer.h>
#include <lagrange/ui/Scene.h>
#include <lagrange/ui/SceneUI.h>
#include <lagrange/ui/Texture.h>
#include <lagrange/ui/UIWidget.h>
#include <lagrange/ui/Viewer.h>
#include <lagrange/ui/SelectionUI.h>
#include <lagrange/utils/strings.h>
#include <nfd.h>
#include <stb_image_write.h>

#include <cstdint>

namespace lagrange {
namespace ui {

namespace {
constexpr float VISIBLE_BUTTON_WIDTH = 28.0f;
constexpr float CLOSE_BUTTON_WIDTH = 24.0f;
constexpr char MODAL_NAME_LOAD_OPTIONS[] = "Load Options";
constexpr char MODAL_NAME_SAVE_OPTIONS[] = "Save Options";
constexpr char MODAL_NAME_NEW_LIGHT_OPTIONS[] = "New Light Options";
}

std::string SceneUI::load_dialog(const std::string& extension /*= "obj"*/) const
{
    nfdchar_t* path = nullptr;
    nfdresult_t result = NFD_OpenDialog(extension.c_str(), nullptr, &path);
    if (result != NFD_OKAY) return "";

    auto path_string = std::string(path);
    free(path);
    return path_string;
}

std::string SceneUI::save_dialog(const std::string& extension /*= "obj"*/) const
{
    nfdchar_t* path = nullptr;
    nfdresult_t result = NFD_SaveDialog(extension.c_str(), nullptr, &path);
    if (result == NFD_OKAY) {
        std::string s = fs::get_string_ending_with(path, ("." + extension).c_str());
        free(path);
        return s;
    }
    return "";
}

void SceneUI::draw_menu()
{
    if (ImGui::MenuItem(ICON_FA_FILE " Open Mesh", "O")) {
        load_mesh_dialog(true);
    }

    if (ImGui::MenuItem(ICON_FA_FILE_IMPORT " Add Mesh", "A")) {
        load_mesh_dialog(false);
    }
}

void SceneUI::draw_models()
{
    auto& scene = get();
    const auto& models = scene.get_models();


    if (get_viewer()->is_key_pressed(GLFW_KEY_O)) load_mesh_dialog(true /* clear scene*/);


    if (!ImGui::CollapsingHeader("Models", ImGuiTreeNodeFlags_DefaultOpen)) return;

    /*
        Scene model actions
    */
    if (ImGui::Button(ICON_FA_FILE " Open Mesh (O)")) load_mesh_dialog(true /* clear scene*/);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Clear the scene and load the model.\nSupported file type: Obj");

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FILE_IMPORT " Add Mesh")) load_mesh_dialog(false);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add the model to the scene.\nSupported file type: Obj");

    ImGui::Separator();

    ImGui::Checkbox("Group by instance", &m_group_by_instance);

    if (models.size() == 0) {
        ImGui::Text("No Models");
    }

    Model* to_save = nullptr;
    Model* to_remove = nullptr;

    auto draw_model_row = [&](Model* model) {
        ImGui::PushID(model);
        if (model->is_visible()) {
            if (button_icon(false, ICON_FA_EYE, "", "", true, ImVec2(VISIBLE_BUTTON_WIDTH, 0.f))) {
                model->set_visible(false);
            }
        } else {
            if (button_icon(false, ICON_FA_EYE_SLASH, "", "", true, ImVec2(VISIBLE_BUTTON_WIDTH, 0.f))) {
                model->set_visible(true);
            }
        }
        ImGui::SameLine();
        float selectable_width = ImGui::GetWindowContentRegionWidth() - ImGui::GetCursorPosX() -
                                 CLOSE_BUTTON_WIDTH * 2 - ImGui::GetStyle().ItemSpacing.x;

        draw_selectable(model, model->get_name(), ImVec2(selectable_width, 0));

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_SAVE, ImVec2(CLOSE_BUTTON_WIDTH, 0.f))) to_save = model;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save model as .obj");

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_TIMES, ImVec2(CLOSE_BUTTON_WIDTH, 0.f))) to_remove = model;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remove model");

        ImGui::PopID();
    };

    /*
        Group by instance
    */
    if (m_group_by_instance) {
        auto groups = get().get_instances();
        int group_counter = 0;
        for (auto& group_it : groups) {
            std::string group_label = string_format("[{}] Model Group", group_counter);

            // Create label based on mesh type
            group_it.second.front()->visit_tuple<SupportedMeshTypes>([&](const auto& model) {
                using ModelType = std::remove_reference_t<decltype(model)>;
                using MeshType = typename ModelType::MeshType;
                using _V = typename MeshType::VertexArray;
                using _F = typename MeshType::FacetArray;

                group_label += string_format(" | {}Mesh{}D<{}>",
                    _F::ColsAtCompileTime == 3 ? "Triangle" : "Quad",
                    _V::ColsAtCompileTime,
                    utils::type_string<typename _V::Scalar>::value().c_str());
            });

            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            if (ImGui::TreeNode(group_label.c_str())) {
                for (auto model_ptr : group_it.second) {
                    draw_model_row(model_ptr);
                }
                ImGui::TreePop();
            }
            group_counter++;
        }
    }
    /*
        List one by one
    */
    else {
        for (auto model_ptr : models) {
            draw_model_row(model_ptr);
        }
    }

    /*
        Removal handling
    */
    if(to_remove) {
        auto& selection = get_viewer()->get_selection().get_global().get_persistent();
        selection.erase(to_remove);
        // TODO: remove ^
        scene.remove_model(to_remove);
    }

    /*
        Save handling
    */
    if (to_save) {
        if (save_mesh_dialog()) {
            m_modal_save_models = {to_save};
        }
    }
}

void SceneUI::draw_materials()
{
    auto& scene = get();

    /*
        Collect and sort materials from meshes
    */
    std::set<Material*> mats;

    for (auto& model : scene.get_models()) {
        for (auto& it : model->get_materials()) {
            mats.insert(it.second.get());
        }
    }

    if (!ImGui::CollapsingHeader("Materials")) return;

    if (mats.size() == 0) {
        ImGui::Text("No materials");
    }

    for (auto mat_ptr : mats) {
        draw_selectable(mat_ptr, mat_ptr->get_name());
    }
}

void SceneUI::draw_emitters()
{
    auto& scene = get();
    auto& emitters = scene.get_emitters();


    if (!ImGui::CollapsingHeader("Emitters")) return;

    /*
        Emitter actions
    */
    if (ImGui::Button(ICON_FA_FILE " Open IBL")) load_ibl_dialog(true /* clear scene*/);
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FILE_IMPORT " Add IBL")) load_ibl_dialog(false);
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FILE_IMPORT " Add Light")) {
        m_modal_to_open = MODAL_NAME_NEW_LIGHT_OPTIONS;
    }

    int to_remove_index = -1;
    int emitter_id = 0;

    auto draw_emitter_row = [&](Emitter* emitter) {
        ImGui::PushID(emitter);
        std::string label;

        switch (emitter->get_type()) {
        case Emitter::Type::POINT: label = string_format("[{}] Point light", emitter_id); break;
        case Emitter::Type::SPOT: label = string_format("[{}] Spot light", emitter_id); break;
        case Emitter::Type::DIRECTIONAL:
            label = string_format("[{}] Directional", emitter_id);
            break;
        case Emitter::Type::IBL:
            label = string_format(
                "[{}] {} (IBL)", emitter_id, reinterpret_cast<IBL*>(emitter)->get_name());
            break;
        default: label = string_format("[{}] Unknown type", emitter_id); break;
        }
        
        if (emitter->is_enabled()) {
            if (button_icon(false, ICON_FA_EYE, "", "", true, ImVec2(VISIBLE_BUTTON_WIDTH, 0.f))) {
                emitter->set_enabled(false);
            }
        } else {
            if (button_icon(false, ICON_FA_EYE_SLASH, "", "", true, ImVec2(VISIBLE_BUTTON_WIDTH, 0.f))) {
                emitter->set_enabled(true);
            }
        }
        ImGui::SameLine();
        float selectable_width =
            ImGui::GetWindowContentRegionWidth() - ImGui::GetCursorPosX() - CLOSE_BUTTON_WIDTH;
        draw_selectable(emitter, label, ImVec2(selectable_width, 0));

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_TIMES, ImVec2(CLOSE_BUTTON_WIDTH, 0))) to_remove_index = emitter_id;

        emitter_id++;

        ImGui::PopID();
    };


    for (auto& emitter_ptr : emitters) {
        draw_emitter_row(emitter_ptr);
    }

    if (to_remove_index != -1) {
        auto emitter_ptr = emitters[to_remove_index];
        get().remove_emitter(emitter_ptr);
    }

    return;
}


void SceneUI::draw_selectable(
    BaseObject* obj,
    const std::string& label /*= "N/A"*/,
    const ImVec2& size)
{
    auto& persistent_selection = get_viewer()->get_selection().get_global().get_persistent();

    if (ImGui::Selectable(label.c_str(), persistent_selection.has(obj), 0, size)) {
        if (m_selectable_erase) {
            persistent_selection.erase(obj);
        } else if (m_selectable_add) {
            persistent_selection.add(obj);
        } else {
            persistent_selection.set(obj);
        }
    }
}


void SceneUI::draw()
{
    this->begin();


    ImGui::Separator();

    // Defaults
    m_selectable_add = ImGui::GetIO().KeyCtrl;
    m_selectable_erase = ImGui::GetIO().KeyAlt;

    //Get modifier status for add/erase from global keybinds
    // These should be the same as those used in the viewport
    auto& keybind_map = get_viewer()->get_keybinds().get();
    {
        auto it = keybind_map.find("viewport.selection.select.add");
        if (it != keybind_map.end() && it->second.size() > 0) {
            m_selectable_add = ImGui::IsKeyDown(it->second.front().modifiers.front());
        }
    }
    {
        auto it = keybind_map.find("viewport.selection.select.erase");
        if (it != keybind_map.end() && it->second.size() > 0) {
            m_selectable_erase = ImGui::IsKeyDown(it->second.front().modifiers.front());
        }
    }

    draw_models();
    draw_materials();
    draw_emitters();

    if (m_modal_to_open.length() > 0) {
        ImGui::OpenPopup(m_modal_to_open.c_str());
        m_modal_to_open = "";
    }


    // Draw popups
    if (ImGui::BeginPopupModal(MODAL_NAME_LOAD_OPTIONS)) {
        ImGui::Checkbox("Normalize", &m_mesh_load_params.normalize);
        ImGui::Checkbox("Load Normals", &m_mesh_load_params.load_normals);
        ImGui::Checkbox("Load Uvs", &m_mesh_load_params.load_uvs);
        ImGui::Checkbox("Load Materials", &m_mesh_load_params.load_materials);
        ImGui::Checkbox("Triangulate", &m_triangulate);
        ImGui::Checkbox("As One Mesh", &m_mesh_load_params.as_one_mesh);


        if (ImGui::Button("Cancel") || get_viewer()->is_key_pressed(GLFW_KEY_ESCAPE)) {
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("(Escape)");

        ImGui::SameLine();

        if (ImGui::Button("Proceed") || get_viewer()->is_key_pressed(GLFW_KEY_ENTER)) {
            if (m_modal_load_clear) get().clear_models();

            if (m_triangulate) {
                auto models =
                    ModelFactory::load_obj<TriangleMesh3D>(m_modal_load_path, m_mesh_load_params);
                get().add_models(std::move(models));
            } else {
                auto models =
                    ModelFactory::load_obj<QuadMesh3D>(m_modal_load_path, m_mesh_load_params);
                get().add_models(std::move(models));
            }

            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("(Enter)");


        ImGui::EndPopup();
    }

    // Draw popups
    if (ImGui::BeginPopupModal(MODAL_NAME_SAVE_OPTIONS)) {
        if (ImGui::Button("Cancel (Escape)") || get_viewer()->is_key_pressed(GLFW_KEY_ESCAPE)) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Save (Enter)") || get_viewer()->is_key_pressed(GLFW_KEY_ENTER)) {
            if (m_modal_save_models.size() == 1) {
                bool has_type = false;
                m_modal_save_models.front()->visit_tuple_const<SupportedMeshTypes>(
                    [&](auto& model) {
                        lagrange::io::save_mesh(m_modal_save_path, model.get_mesh());
                        has_type = true;
                    });

                if (has_type) {
                    ImGui::CloseCurrentPopup();
                } else {
                    ImGui::TextColored(ImColor(ImGui::Spectrum::RED500).Value,
                        "Couldn't save mesh, unsupported type.");
                }
            } else {
                LA_ASSERT("Multiple mesh save not implemented yet");
            }
        }

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal(MODAL_NAME_NEW_LIGHT_OPTIONS)) {
        if (ImGui::Button("Spot light")) {
            get().add_emitter(std::make_unique<SpotLight>(Eigen::Vector3f(0, 5, 0),
                Eigen::Vector3f(0, -1, 0),
                Eigen::Vector3f::Constant(50.0f)));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();

        if (ImGui::Button("Directional light")) {
            get().add_emitter(std::make_unique<DirectionalLight>(
                Eigen::Vector3f(0, -1, 0), Eigen::Vector3f::Constant(0.5f)));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();

        if (ImGui::Button("Point light")) {
            get().add_emitter(
                std::make_unique<PointLight>(Eigen::Vector3f(0, 5, 0), Eigen::Vector3f::Constant(50.0f)));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        ImGui::EndPopup();
    }


    this->end();
}

io::MeshLoaderParams& SceneUI::get_mesh_load_params()
{
    return m_mesh_load_params;
}

void SceneUI::load_mesh_dialog(bool clear)
{
    auto path = load_dialog();
    if (path.length() > 0) {
        m_modal_load_path = path;
        m_modal_load_clear = clear;
        m_modal_to_open = MODAL_NAME_LOAD_OPTIONS;
    }
}

bool SceneUI::save_mesh_dialog()
{
    auto path = save_dialog();
    if (path.length() > 0) {
        m_modal_save_path = path;
        m_modal_to_open = MODAL_NAME_SAVE_OPTIONS;
        return true;
    }
    return false;
}

void SceneUI::load_ibl_dialog(bool clear)
{
    auto& scene = get();
    auto path = load_dialog("ibl,jpg,hdr,png");
    if (path.length() > 0) {
        if (clear) scene.clear_emitters(Emitter::Type::IBL);

        scene.add_emitter(std::make_unique<IBL>(path));
    }
}


} // namespace ui
} // namespace lagrange

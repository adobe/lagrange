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
///IMGUIZMO INCLUDE ORDER
#include <imgui.h>
////
#ifndef NULL
#define NULL nullptr
#endif
#include <ImGuizmo.h>
////

#include <lagrange/ui/SelectionUI.h>

#include <lagrange/ui/Viewer.h>
#include <lagrange/ui/Scene.h>
#include <lagrange/ui/Renderer.h>
#include <lagrange/ui/ViewportUI.h>
#include <lagrange/ui/Viewport.h>
#include <lagrange/ui/Keybinds.h>
#include <lagrange/ui/MeshModelBase.h>
#include <lagrange/ui/Resource.h>
#include <lagrange/ui/MeshBufferFactory.h>
#include <lagrange/ui/Camera.h>

#include <lagrange/utils/warning.h>

#include <IconsFontAwesome5.h>

namespace lagrange {
namespace ui {

SelectionUI::SelectionUI(Viewer* viewer)
    : UIPanelBase(viewer)
{

    /*
        Setup selection
        There are two selections:
            m_base_object_selection - global selection, mostly for models, emitters, etc.
            Model::m_element_selection - model specific selection (for vertex, edge, face)
        The mode is controlled by
            m_selection_mode
    */

    m_global_selection = std::make_unique<TwoStateSelection<BaseObject*>>();

    // Remove models from selected when they are removed from scene
    auto& scene = viewer->get_scene();
    scene.add_callback<Scene::OnModelRemove>([&](Model& model) {
        get_global().get_persistent().erase(&model);
        get_global().get_transient().erase(&model);
    });
    scene.add_callback<Scene::OnEmitterRemove>(
        [&](Emitter& emitter) {
        get_global().get_persistent().erase(&emitter);
        get_global().get_transient().erase(&emitter);
    });


    auto update_selection_buffer = [](Model& model, bool persistent, SelectionElementType type) {
        
        LA_IGNORE(type);
        auto buffer = model.get_buffer();
        if (!buffer) return;
        MeshBufferFactory::update_selection_indices(
            reinterpret_cast<MeshModelBase&>(model).get_proxy_mesh(),
            model.get_selection(),
            persistent,
            *buffer);
    };

    // Make sure element selection in model updates render buffers
    scene.add_callback<Scene::OnModelAdd>([update_selection_buffer](Model& model) {
        auto mesh_model_ptr = dynamic_cast<MeshModelBase*>(&model);
        if (!mesh_model_ptr) return;

        model.add_callback<Model::OnSelectionChange>(update_selection_buffer);
    });

    set_selection_mode(SelectionElementType::OBJECT);
}

void SelectionUI::update(double )
{
    // TODO: there should be a more principled way to do this, but for now this will do...
    bool no_guizmo = !ImGuizmo::IsUsing() && !ImGuizmo::IsOver();

    if (no_guizmo) {
        // Update selection in renderer
        // TODO: move this in the renderer itself? (query as needed)
        get_viewer()->get_renderer().update_selection(*this);

        interaction();
    }
}

void SelectionUI::interaction()
{
    m_selection_changed = false;

    auto& keys = get_viewer()->get_keybinds();
    auto& viewport = get_viewer()->get_focused_viewport_ui();
    auto& scene = get_viewer()->get_scene();
    auto& camera = viewport.get().get_camera();
    if (!viewport.is_selection_enabled()) return;

    const bool selection_keys_down = keys.is_down("viewport.selection.select.set") ||
                                     keys.is_down("viewport.selection.select.add") ||
                                     keys.is_down("viewport.selection.select.erase");

    SelectionBehavior sel_behavior = SelectionBehavior::SET;
    if (keys.is_down("viewport.selection.select.erase")) {
        sel_behavior = SelectionBehavior::ERASE;
    } else if (keys.is_down("viewport.selection.select.add")) {
        sel_behavior = SelectionBehavior::ADD;
    }

    static ImVec2 mouse_down(0, 0); // mouse position at interaction start
    static bool selection_active = false;
    bool starting_now = false;

    static ImVec2 mouse_previous(0, 0); // mouse position at previous frame
    bool needs_update = true;
    ImVec2 mouse_now = ImGui::GetMousePos();
    if (mouse_now.x == mouse_previous.x && mouse_now.y == mouse_previous.y) {
        needs_update = false;
    }
    mouse_previous = mouse_now;

    if (!selection_active && selection_keys_down) {
        // start of interaction
        selection_active = true;
        mouse_down = ImGui::GetMousePos();
        starting_now = true;
        needs_update = true;
    }

    if (selection_active && !selection_keys_down) {
        // end of interaction
        selection_active = false;
    }

    if (selection_active && needs_update) {
        Eigen::Vector2f begin;
        Eigen::Vector2f end;
        if (m_painting_selection) {
            auto center = viewport.screen_to_viewport(ImGui::GetMousePos());
            begin = center - Eigen::Vector2f::Constant(m_painting_selection_radius);
            end = center + Eigen::Vector2f::Constant(m_painting_selection_radius);
        } else {
            begin = viewport.screen_to_viewport(mouse_down);
            end = viewport.screen_to_viewport(ImGui::GetMousePos());
            if (end.x() < begin.x()) std::swap(end.x(), begin.x());
            if (end.y() < begin.y()) std::swap(end.y(), begin.y());
        }

        // Note: while dragging, we keep selecting and unselecting
        // elements or objects, which keeps firing on-selection-changed events.
        // We should be using on-hover while dragging, and only selecting on mouse up.


        std::vector<Model*> models;
        if (begin.x() == end.x() && begin.y() == end.y()) {
            auto* model = scene.get_model_at(camera, begin);
            if (model) models.push_back(model);
        } else {
            models = scene.get_models_in_region(camera, begin, end);
        }


        // in painting mode, don't set after the first click
        if (m_painting_selection && sel_behavior == SelectionBehavior::SET && !starting_now) {
            sel_behavior = SelectionBehavior::ADD;
        }

        // in painting mode + set + element, clear all objects on first click
        if (m_painting_selection && sel_behavior == SelectionBehavior::SET && starting_now &&
            m_selection_mode != SelectionElementType::OBJECT) {

            for (auto* model : scene.get_models()) {
                m_selection_changed |= model->get_selection().get_persistent().clear();
            }
        }


        // if a model that we previously selected is not in the active selection, clear it.
        // Note: we could just loop over the whole scene every time, this is just a simple
        // optimization. It shouldn't matter on simple scenes.
        // Improve if this becomes a problem for larger scenes.
        if (!m_painting_selection && sel_behavior == SelectionBehavior::SET &&
            m_selection_mode != SelectionElementType::OBJECT) {

            static std::vector<Model*> previous_models;
            if (starting_now) previous_models = scene.get_models();
            if (!previous_models.empty()) {
                for (auto* model : previous_models) {
                    if (std::find(models.begin(), models.end(), model) == models.end()) {
                        m_selection_changed |= model->get_selection().get_persistent().clear();
                    }
                }
            }
            previous_models = models;
        }

        if (m_selection_mode == SelectionElementType::OBJECT) {
            m_selection_changed |=
                m_global_selection->get_persistent().update_multiple(models, sel_behavior);

        } else {
            for (auto* model : models) {
                auto* meshmodel = dynamic_cast<MeshModelBase*>(model);
                if (meshmodel) {
                    m_selection_changed |= select_elements(camera, meshmodel, begin, end, sel_behavior);
                }
            }
        }

    }

    if (viewport.hovered()) {
        const ImColor bg(1.0f, 1.0f, 1.0f, 0.05f);
        const ImColor border(1.0f, 1.0f, 1.0f, 0.25f);
        if (m_painting_selection) {
            const ImVec2 center = ImGui::GetMousePos();
            const float radius = m_painting_selection_radius;
            const ImVec2 tl(center.x - radius, center.y - radius);
            const ImVec2 br(center.x + radius, center.y + radius);
            ImGui::GetForegroundDrawList()->AddRectFilled(tl, br, bg);
            ImGui::GetForegroundDrawList()->AddRect(tl, br, border);
        } else if (selection_active && selection_keys_down) {
            ImGui::GetForegroundDrawList()->AddRectFilled(mouse_down, ImGui::GetMousePos(), bg);
            ImGui::GetForegroundDrawList()->AddRect(mouse_down, ImGui::GetMousePos(), border);
        }
    }
}

bool SelectionUI::select_elements(const Camera& camera,
    MeshModelBase* model,
    Eigen::Vector2f begin,
    Eigen::Vector2f end,
    SelectionBehavior behavior)
{

    auto& model_selection = model->get_selection().get_persistent();

    if (model->get_selection().get_type() != m_selection_mode) {
        model->get_selection().get_persistent().clear();
        model->get_selection().get_transient().clear();
        model->get_selection().set_type(m_selection_mode);
    }

    if (end.x() - begin.x() < 2.0f && end.y() - begin.y() < 2.0f) {
        int elem_id = -1;

        if (m_selection_mode == SelectionElementType::FACE) {
            model->get_facet_at(camera, begin, elem_id);
        } else if (m_selection_mode == SelectionElementType::VERTEX) {
            model->get_vertex_at(camera, begin, 40.0f, elem_id);
        } else if (m_selection_mode == SelectionElementType::EDGE) {
            model->get_edge_at(camera, begin, 40.0f, elem_id);
        }

        bool changed = false;
        if (elem_id == -1) {
            if (behavior == SelectionBehavior::SET) {
                changed |= model_selection.clear();
            }
        } else {
            changed |= model_selection.update(elem_id, elem_id != -1, behavior);
        }
        return changed;
    } else {
        std::unordered_set<int> result;

        if (m_selection_mode == SelectionElementType::FACE) {
            result = model->get_facets_in_frustum(camera, begin, end, !m_select_backfaces);
        } else if (m_selection_mode == SelectionElementType::EDGE) {
            result = model->get_edges_in_frustum(camera, begin, end, !m_select_backfaces);
        } else if (m_selection_mode == SelectionElementType::VERTEX) {
            result = model->get_vertices_in_frustum(camera, begin, end, !m_select_backfaces);
        }
        return model_selection.update_multiple(result, behavior);
    }
}

bool SelectionUI::draw_toolbar() {

    bool enable_element = true;

    if (button_toolbar(get_selection_mode() == SelectionElementType::OBJECT,
            ICON_FA_CUBES,
            "Object",
            "global.selection_mode.object",
            true)) {
        set_selection_mode(SelectionElementType::OBJECT);
    }

    if (button_toolbar(get_selection_mode() == SelectionElementType::FACE,
            ICON_FA_CUBE,
            "Face",
            "global.selection_mode.face",
            enable_element)) {
        set_selection_mode(SelectionElementType::FACE);
    }
    if (button_toolbar(get_selection_mode() == SelectionElementType::EDGE,
            ICON_FA_ARROWS_ALT_H,
            "Edge",
            "global.selection_mode.edge",
            enable_element)) {
        set_selection_mode(SelectionElementType::EDGE);
    }
    if (button_toolbar(get_selection_mode() == SelectionElementType::VERTEX,
            ICON_FA_CIRCLE,
            "Vertex",
            "global.selection_mode.vertex",
            enable_element)) {
        set_selection_mode(SelectionElementType::VERTEX);
    }

    if (button_toolbar(m_select_backfaces,
            ICON_FA_STEP_BACKWARD,
            "Select Backfaces",
            "global.selection_mode.select_backfaces",
            true)) {
        m_select_backfaces = !m_select_backfaces;
    }
    if (button_toolbar(m_painting_selection,
            ICON_FA_PAINT_ROLLER,
            "Paint Selection (right click for details)",
            "global.selection_mode.paint_selection",
            true)) {
        m_painting_selection = !m_painting_selection;
    }
    if (ImGui::BeginPopupContextItem("Paint Selection")) {
        ImGui::SliderFloat("Radius", &m_painting_selection_radius, 1.0f, 500.0f, "%.1f");
        ImGui::EndPopup();
    }

    return true;
}

void SelectionUI::set_selection_mode(SelectionElementType mode)
{
    auto* viewer = get_viewer();
    auto& scene = viewer->get_scene();
    auto& renderer = viewer->get_renderer();

    if (mode != m_selection_mode) {
        /*
            Reset invidiual model selection if needed
        */
        for (auto* obj : get_viewer()->get_scene().get_models()) {
            auto model = dynamic_cast<Model*>(obj);
            if (!model) continue;
            model->get_selection().set_type(mode);
        }
    }

    // HACK: if going into element selection, select all objects in the scene.
    // This is not needed by selection itself, but viz doesn't update properly without.
    // TODO: figure out and fix.
    if (mode != SelectionElementType::OBJECT) {
        m_global_selection->get_persistent().set_multiple(scene.get_models(), false);
    }

    /*
        Toggle render passes
    */

    for (auto viewport_ui : get_viewer()->get_viewports()) {
        auto& viewport = viewport_ui->get();

        viewport.enable_render_pass(
            renderer.get_default_pass<PASS_OBJECT_OUTLINE>(), mode == SelectionElementType::OBJECT);

        viewport.enable_render_pass(
            renderer.get_default_pass<PASS_SELECTED_FACET>(), mode == SelectionElementType::FACE);

        viewport.enable_render_pass(
            renderer.get_default_pass<PASS_SELECTED_EDGE>(), mode == SelectionElementType::EDGE);

        viewport.enable_render_pass(renderer.get_default_pass<PASS_SELECTED_VERTEX>(),
            mode == SelectionElementType::VERTEX);

        // display edges in edge or facet selection mode
        viewport.enable_render_pass(renderer.get_default_pass<PASS_EDGE>(),
            (mode == SelectionElementType::FACE) || (mode == SelectionElementType::EDGE));

        // display vertices in vertex selection mode
        viewport.enable_render_pass(
            renderer.get_default_pass<PASS_VERTEX>(), mode == SelectionElementType::VERTEX);
    }

    m_selection_mode = mode;
    m_callbacks.call<OnSelectionElementTypeChange>(mode);
}

}
}

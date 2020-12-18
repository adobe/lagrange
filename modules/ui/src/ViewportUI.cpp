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

///IMGUIZMO INCLUDE ORDER
#include <imgui.h>
////
#ifndef NULL
#define NULL nullptr
#endif
#include <ImGuizmo.h>
////

#include <lagrange/ui/GLContext.h>
#include <lagrange/ui/MeshModel.h>
#include <lagrange/ui/MeshModelBase.h>
#include <lagrange/ui/SelectionUI.h>
#include <lagrange/ui/UIWidget.h>
#include <lagrange/ui/Viewer.h>
#include <lagrange/ui/ViewportUI.h>
#include <lagrange/utils/la_assert.h>
#include <lagrange/utils/strings.h>
#include <misc/cpp/imgui_stdlib.h>


namespace lagrange {
namespace ui {


int ViewportUI::m_viewport_ui_counter = 0;


ViewportUI::ViewportUI(Viewer* viewer, const std::shared_ptr<Viewport>& viewport)
    : UIPanel(viewer, viewport)
{
    LA_ASSERT(viewport, "Viewport must be set");
    m_id = ViewportUI::m_viewport_ui_counter++;

    Eigen::Vector2f pos = screen_to_viewport(ImGui::GetMousePos());

    auto t = std::chrono::high_resolution_clock::now();
    for (auto i = 0; i < max_mouse_buttons; i++) {
        m_last_mouse_click[i] = pos;
        m_last_mouse_click_time[i] = t;
        m_last_mouse_release_time[i] = t;
    }
}

void ViewportUI::draw()
{
    auto& viewport = get();

    ImGui::SetNextWindowSize(
        ImVec2(float(viewport.get_width()), float(viewport.get_height())), ImGuiCond_FirstUseEver);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    this->begin(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    ImGuizmo::Enable(true);

    ImGui::PushID(m_id);

    m_hovered = !ImGuizmo::IsUsing() && ImGui::IsWindowHovered(ImGuiHoveredFlags_RootWindow);

    /*
        Draw render pass etc. options
    */
    draw_options();

    /*
        Draw toolbar that controls the viewport
    */
    draw_viewport_toolbar();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    /*
        Calculate canvas position
    */
    m_avail_width = std::max(16, static_cast<int>(ImGui::GetContentRegionAvail().x));
    m_avail_height = std::max(16, static_cast<int>(ImGui::GetContentRegionAvail().y - 2));
    m_canvas_pos = ImGui::GetCursorScreenPos();

    viewport.set_dimensions(m_avail_width, m_avail_height);

    const bool in_texture_rect = [&]() {
        const Eigen::Vector2f pos = get_viewer()->get_mouse_pos();
        return (pos.x() > m_canvas_pos.x() && pos.y() > m_canvas_pos.y() &&
                pos.x() < m_canvas_pos.x() + m_avail_width &&
                pos.y() < m_canvas_pos.y() + m_avail_height);
    }();

    m_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootWindow) && in_texture_rect;


    /*
        Display texture
    */
    {
        const auto texture_ptr = viewport.get_framebuffer().get_color_attachement(0);
        const ImTextureID texID = reinterpret_cast<void*>((long long int)(texture_ptr->get_id()));

        const ImVec2 uv0 = ImVec2(0, 1);
        const ImVec2 uv1 = ImVec2(1, 0);

        ImGui::Image(texID,
            ImVec2(float(viewport.get_width()), float(viewport.get_height())),
            uv0,
            uv1,
            ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
            ImVec4(1.0f, 1.0f, 1.0f, 0.5f));
    }

    /*
        Mouse/key interaction
    */
    // Note: interaction() calls ImGuizmo to draw the gizmo on this window's drawlist, so to draw
    // on top of the viewport, this needs to happen after the above ImGui::Image(..) call.
    ImGuizmo::SetDrawlist();
    interaction();

    ImGui::PopID();
    this->end();
    ImGui::PopStyleVar();
}

void ViewportUI::update(double dt)
{

    auto& scene = get_viewer()->get_scene();
    auto& cam = get_viewport().get_camera();

    /*
        Camera near/far update
    */
    if (m_auto_nearfar) {
        AABB bb = scene.get_bounds();
        AABB immediate_bounds = get_viewer()->get_renderer().immediate_data_bounds();

        if (!immediate_bounds.isEmpty()) {
            bb.extend(immediate_bounds);
        }

        if (!bb.isEmpty()) {
            // Expand by camera position
            bb.extend(cam.get_position());

            float nearest = scene.get_nearest_bounds_distance(cam.get_position());
            float furthest = scene.get_furthest_bounds_distance(cam.get_position());

            float near_plane = cam.get_near();
            float far_plane = cam.get_far();
            if (nearest != std::numeric_limits<float>::max() &&
                cam.get_type() == Camera::Type::PERSPECTIVE) {
                near_plane = std::max(0.5f * nearest, 0.000001f);
            }

            if (furthest != std::numeric_limits<float>::min() &&
                cam.get_type() == Camera::Type::PERSPECTIVE) {
                far_plane = 2.0f * furthest;
            }

            if (nearest != std::numeric_limits<float>::max() &&
                cam.get_type() == Camera::Type::PERSPECTIVE) {
                near_plane = std::max(0.5f * nearest, 0.000001f);
            }

            cam.set_planes(near_plane, far_plane);
        }
    }


    UIPanelBase::update(dt);
}

const char* ViewportUI::get_title() const
{
    static std::string name = string_format("Viewport {}", m_id);
    return name.c_str();
}


Eigen::Vector2f ViewportUI::screen_to_viewport(const Eigen::Vector2f& pos)
{
    return {pos.x() - m_canvas_pos.x(),
        get().get_camera().get_window_height() - (pos.y() - m_canvas_pos.y())};
}

Viewport& ViewportUI::get_viewport()
{
    return get();
}

const Viewport& ViewportUI::get_viewport() const
{
    return get();
}


void ViewportUI::draw_viewport_toolbar()
{
    // Add padding, as window padding is disabled
    ImGui::Dummy(ImVec2(5, 1));
    ImGui::Dummy(ImVec2(1, 5));
    ImGui::SameLine();

    auto& viewport = get_viewport();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    auto separator = [=]() {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        draw_list->AddLine(ImVec2(p.x, p.y),
            ImVec2(p.x, p.y + ImGui::GetTextLineHeightWithSpacing()),
            ImColor(ImGui::Spectrum::GRAY300),
            1.0f);
        ImGui::Dummy(ImVec2(2.5f, 0));
        ImGui::SameLine();
    };


    bool pbr = viewport.is_render_pass_enabled_tag("pbr");
    bool outline = viewport.is_render_pass_enabled_tag("outline");

    /*
        Render appearence
    */

    if (button_icon(pbr, ICON_FA_LIGHTBULB, "Physically Based Render")) {
        viewport.enable_render_pass_tag("pbr", !pbr);
    }
    ImGui::SameLine();

    bool edges = viewport.is_render_pass_enabled("Edges");
    bool vertices = viewport.is_render_pass_enabled("Vertices");

    if (button_icon(edges, ICON_FA_ARROWS_ALT_H, "Edges")) {
        viewport.enable_render_pass("Edges", !edges);
    }
    ImGui::SameLine();
    if (button_icon(vertices, ICON_FA_CIRCLE, "Vertices")) {
        viewport.enable_render_pass("Vertices", !vertices);
    }
    ImGui::SameLine();
    bool normal = viewport.is_render_pass_enabled_tag("normal");
    if (button_icon(normal, ICON_FA_EXTERNAL_LINK_ALT, "Normals")) {
        viewport.enable_render_pass_tag("normal", !normal);
    }
    ImGui::SameLine();
    bool bbox = viewport.is_render_pass_enabled_tag("boundingbox");
    if (button_icon(bbox, ICON_FA_VECTOR_SQUARE, "Bounding Box")) {
        viewport.enable_render_pass_tag("boundingbox", !bbox);
    }
    ImGui::SameLine();
    ImGui::PushID("normals");
    if (ImGui::Button(ICON_FA_CARET_DOWN)) {
        ImGui::OpenPopup("Normals");
    }
    ImGui::SameLine();


    if (ImGui::BeginPopupContextItem("Normals")) {
        auto pass = viewport.get_renderer().get_pipeline().get_pass(
            default_render_pass_name<NormalsPass>());
        auto& opt = pass->get_options();

        float w = 250.0f;
        bool face = opt["Face"].get<bool>("Enabled");
        if (ImGui::Selectable(ICON_FA_EXTERNAL_LINK_ALT " Face Normals", face, 0, ImVec2(w, 0))) {
            opt["Face"].set<bool>("Enabled", !face);
        }

        bool corner = opt["Corner Vertex"].get<bool>("Enabled");
        if (ImGui::Selectable(ICON_FA_EXTERNAL_LINK_SQUARE_ALT "   Vertex Corner Normals",
                corner,
                0,
                ImVec2(w, 0))) {
            opt["Corner Vertex"].set<bool>("Enabled", !corner);
        }

        bool pervertex = opt["Per-Vertex"].get<bool>("Enabled");
        if (ImGui::Selectable(
                ICON_FA_ASTERISK "  Per-Vertex Normals", pervertex, 0, ImVec2(w, 0))) {
            opt["Per-Vertex"].set<bool>("Enabled", !pervertex);
        }


        ImGui::EndPopup();
    }

    ImGui::PopID();


    separator();

    /*
        Post processing
    */
    if (button_icon(outline, ICON_FA_OBJECT_GROUP, "Selection Outline")) {
        viewport.enable_render_pass_tag("outline", !outline);
    }
    ImGui::SameLine();
    bool fxaa = viewport.is_render_pass_enabled(default_render_pass_name<FXAAPass>());
    if (button_icon(fxaa, "AA", "FXAA")) {
        viewport.enable_render_pass(default_render_pass_name<FXAAPass>(), !fxaa);
    }
    ImGui::SameLine();

    separator();

    /*
        Details
    */
    if (ImGui::Button("Render passes " ICON_FA_CARET_DOWN)) {
        auto popup_name = "##popup" + std::string(get_title());
        ImGui::OpenPopup(popup_name.c_str());
    }
    ImGui::SameLine();


    separator();


    /*
        Camera
    */
    auto& cam = get_viewport().get_camera();
    bool perspective = (cam.get_type() == Camera::Type::PERSPECTIVE);
    bool ortho = (cam.get_type() == Camera::Type::ORTHOGRAPHIC);

    if (button_icon(perspective, ICON_FA_LESS_THAN, "Perspective")) {
        if (!perspective) cam.set_type(Camera::Type::PERSPECTIVE);
    }
    ImGui::SameLine();
    if (button_icon(ortho, ICON_FA_BARS, "Orthographic")) {
        if (!ortho) cam.set_type(Camera::Type::ORTHOGRAPHIC);
    }
    ImGui::SameLine();
    if (ortho) {
        if (button_icon(
                m_ortho_interaction_2D, "2D", "2D Orthographic Camera Interaction")) {
            m_ortho_interaction_2D = !m_ortho_interaction_2D;
        }
        ImGui::SameLine();
        if (button_icon(
                !m_ortho_interaction_2D, "3D", "3D Orthographic Camera Interaction")) {
            m_ortho_interaction_2D = !m_ortho_interaction_2D;
        }
        ImGui::SameLine();
    }

    if (button_icon(
            cam.is_orthogonal_direction(Camera::Dir::TOP), "Top", "Top-Down View")) {
        cam.set_orthogonal_direction(Camera::Dir::TOP);
    }
    ImGui::SameLine();
    if (button_icon(
            cam.is_orthogonal_direction(Camera::Dir::BOTTOM), "Down", "Up View")) {
        cam.set_orthogonal_direction(Camera::Dir::BOTTOM);
    }
    ImGui::SameLine();
    if (button_icon(
            cam.is_orthogonal_direction(Camera::Dir::FRONT), "Front", "Front View")) {
        cam.set_orthogonal_direction(Camera::Dir::FRONT);
    }
    ImGui::SameLine();
    if (button_icon(
            cam.is_orthogonal_direction(Camera::Dir::LEFT), "Left", "Left View")) {
        cam.set_orthogonal_direction(Camera::Dir::LEFT);
    }
    ImGui::SameLine();
    if (button_icon(
            cam.is_orthogonal_direction(Camera::Dir::BACK), "Back", "Back View")) {
        cam.set_orthogonal_direction(Camera::Dir::BACK);
    }
    ImGui::SameLine();
    if (button_icon(
            cam.is_orthogonal_direction(Camera::Dir::RIGHT), "Right", "Right View")) {
        cam.set_orthogonal_direction(Camera::Dir::RIGHT);
    }
    ImGui::SameLine();

    if (button_icon(false, ICON_FA_SHARE_SQUARE, "Instance camera to other viewports")) {
        for (auto& ui_panel : get_viewer()->get_ui_panels()) {
            auto viewport_other = dynamic_cast<ViewportUI*>(ui_panel.get());
            if (!viewport_other) continue;
            viewport_other->get_viewport().set_camera_ptr(get().get_camera_ptr());
        }
    }
    ImGui::SameLine();

    if (button_icon(false, ICON_FA_SHARE, "Copy camera to other viewports")) {
        for (auto& ui_panel : get_viewer()->get_ui_panels()) {
            auto viewport_other = dynamic_cast<ViewportUI*>(ui_panel.get());
            if (!viewport_other) continue;
            viewport_other->get_viewport().set_camera_ptr(
                std::make_shared<Camera>(get().get_camera()));
        }
    }
    ImGui::SameLine();

    if (button_icon(false,
            ICON_FA_COMPRESS_ARROWS_ALT,
            "Focus On Selection",
            "global.camera.center_on_selection")) {
        Eigen::Vector3f focus = Eigen::Vector3f::Zero();
        int pos_count = 0;

        auto& selection = get_viewer()->get_selection();
        auto& obj_sel = selection.get_global().get_persistent().get_selection();

        if (obj_sel.size() == 0) {
        } else if (obj_sel.size() == 1 &&
                   selection.get_selection_mode() != SelectionElementType::OBJECT) {
            auto model_ptr = dynamic_cast<const Model*>(*obj_sel.begin());

            if (model_ptr) {
                if (model_ptr->get_selection().get_persistent().size() > 0) {
                    focus = model_ptr->get_selection_bounds().center();
                } else {
                    model_ptr->get_bounds().center();
                }
            }

        } else {
            for (auto obj : obj_sel) {
                auto model = dynamic_cast<const Model*>(obj);
                if (!model) continue;
                focus += model->get_bounds().center();
                pos_count++;
            }
            if (pos_count) focus *= (1.0f / pos_count);
        }

        cam.set_lookat(focus);
        cam.set_up(Eigen::Vector3f(0, 1, 0));
    }
    ImGui::SameLine();

    separator();

    bool ground_pass = viewport.is_render_pass_enabled(default_render_pass_name<GroundPass>());
    if (button_icon(ground_pass, ICON_FA_TH, "Toggle Ground")) {
        viewport.enable_render_pass(default_render_pass_name<GroundPass>(), !ground_pass);
    }

    ImGui::SameLine();

    // Ground options
    {
        ImGui::PushID("groundopt");
        if (ImGui::Button(ICON_FA_CARET_DOWN)) {
            ImGui::OpenPopup("Ground");
        }
        ImGui::SameLine();

        if (ImGui::BeginPopupContextItem("Ground")) {
            auto& renderer = viewport.get_renderer();
            ImGui::Dummy(ImVec2(400, 0));
            UIWidget("Grid Options")(
                renderer.get_default_pass<DefaultPasses::PASS_GROUND>()->get_options(),
                "",
                0,
                false);

            ImGui::EndPopup();
        }

        ImGui::PopID();
    }


    if (button_icon(m_auto_nearfar, ICON_FA_COMPRESS, "Automatic Clipping Planes")) {
        m_auto_nearfar = !m_auto_nearfar;
    }

    separator();

    ImGui::Dummy(ImVec2(0, 0));
}

void ViewportUI::draw_options()
{
    auto popup_name = "##popup" + std::string(get_title());


    if (ImGui::BeginPopupContextItem(popup_name.c_str())) {
        if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();

        ImGui::Dummy(ImVec2(400, 0));

        auto& viewport = get();
        auto& renderer = viewport.get_renderer();
        auto& all_passes = renderer.get_pipeline().get_passes();


        auto display_pass = [&](RenderPassBase* pass) {
            const auto& display_name = pass->get_name();
            bool enabled = viewport.is_render_pass_enabled(pass);

            if (ImGui::Checkbox(display_name.c_str(), &enabled)) {
                viewport.enable_render_pass(pass, enabled);
            }

            for (const auto& tag : pass->get_tags()) {
                ImGui::SameLine();
                ImGui::TextColored(ImColor(ImGui::Spectrum::GRAY500).Value, "[%s]", tag.c_str());
            }

            if (pass->get_options().get_children().size() > 0 ||
                pass->get_options().get_options().size() > 0) {
                ImGui::SameLine();
                if (ImGui::TreeNode(("##" + display_name).c_str())) {
                    UIWidget()(pass->get_options());

                    auto deps = renderer.get_pipeline().get_dependencies({pass});
                    if (deps.size() > 0) {
                        ImGui::Separator();
                        ImGui::Text("Dependencies (%d):", int(deps.size()));
                        for (auto* dep : deps) {
                            ImGui::Text("\t%s", dep->get_name().c_str());
                        }
                    }

                    ImGui::TreePop();
                }
            }
        };

        if (ImGui::TreeNode("Default")) {
            for (auto& pass_ptr : all_passes) {
                if (!pass_ptr->has_tag("default")) continue;
                display_pass(pass_ptr.get());
            }
            ImGui::TreePop();
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Custom")) {
            for (auto& pass_ptr : all_passes) {
                if (pass_ptr->has_tag("default")) continue;
                display_pass(pass_ptr.get());
            }
            ImGui::TreePop();
        }

        ImGui::EndPopup();
    }
}

void ViewportUI::interaction()
{
    // Handle mouse interaction
    const auto& io = ImGui::GetIO();
    auto& viewport = get();
    auto& cam = viewport.get_camera();
    auto& keys = get_viewer()->get_keybinds();
    auto& object_selection = get_viewer()->get_selection().get_global().get_persistent();
    if (object_selection.size() > 0) {
        gizmo(object_selection);
    }

    m_last_mouse_pos = screen_to_viewport(ImGui::GetMousePos());

    bool hovered_no_gizmo = hovered() && !ImGuizmo::IsUsing() && !ImGuizmo::IsOver();

    if (hovered_no_gizmo) {
        // Save clicked positions
        for (auto i = 0; i < max_mouse_buttons; i++) {
            if (ImGui::IsMouseClicked(i)) {
                m_last_mouse_click[i] = m_last_mouse_pos;
            }
        }

        if (keys.is_pressed("viewport.camera.rotate")) {
            auto p = screen_to_viewport(ImGui::GetMousePos());

            if (!m_rotation_active) {
                m_rotation_mouse_start = p;
                m_rotation_camera_pos_start = cam.get_position();
                m_rotation_camera_up_start = cam.get_up();
            }
            m_rotation_active = true;
        }

        // Zoom using mousewheel
        if (io.MouseWheel != 0.0f) {
            float delta = static_cast<float>(0.15f * io.MouseWheel);
            if (io.KeyShift) delta *= 0.1f;
            zoom(delta, {io.MousePos.x, io.MousePos.y});
        }
    }

    // Zoom using dolly keybind
    if (keys.is_down("viewport.camera.dolly")) {
        float delta = float(get_viewer()->get_mouse_delta().x()) / cam.get_window_width();
        zoom(delta, m_last_mouse_pos);
    }

    // Mouse released anywhere
    if (keys.is_released("viewport.camera.rotate")) {
        m_rotation_active = false;
    }

    if (keys.is_released("viewport.camera.dolly")) {
        m_dolly_active = false;
    }

    if (keys.is_down("viewport.camera.pan")) {
        Eigen::Vector2f dscreen = (get_viewer()->get_mouse_delta() * 0.01f);
        Eigen::Vector3f side = cam.get_direction().cross(cam.get_up()).normalized();
        Eigen::Vector3f dpos = cam.get_up() * dscreen.y() + side * -dscreen.x();
        cam.set_position(cam.get_position() + dpos);
        cam.set_lookat(cam.get_lookat() + dpos);
    }


    if (m_rotation_active && keys.is_down("viewport.camera.rotate")) {
        rotate_camera();
    }
}

void ViewportUI::zoom(float delta, Eigen::Vector2f screen_pos)
{
    auto& cam = get().get_camera();

    if (!m_ortho_interaction_2D) {
        cam.zoom(delta);
    } else {
        Eigen::Vector2f origin_window =
            Eigen::Vector2f(screen_pos.x() / float(cam.get_window_width()),
                screen_pos.y() / float(cam.get_window_height()));

        Eigen::Vector4f viewport_adjust = Eigen::Vector4f(
            origin_window.x(), origin_window.x(), origin_window.y(), origin_window.y());
        Eigen::Vector4f v = cam.get_ortho_viewport();
        v -= viewport_adjust;
        cam.set_ortho_viewport((v * (1.0f - delta / 5.0f)) + viewport_adjust);
    }
}

size_t ViewportUI::select_objects(
    bool persistent, Eigen::Vector2f begin, Eigen::Vector2f end, SelectionBehavior behavior)
{
    if (!persistent) return 0;
    auto& global_selection = persistent
                                 ? get_viewer()->get_selection().get_global().get_persistent()
                                 : get_viewer()->get_selection().get_global().get_transient();

    auto& viewport = get();
    auto& scene = viewport.get_scene();
    const auto& camera = viewport.get_camera();

    if (end.x() < begin.x()) std::swap(end.x(), begin.x());
    if (end.y() < begin.y()) std::swap(end.y(), begin.y());

    if (begin.x() == end.x() && begin.y() == end.y()) {
        auto* model = scene.get_model_at(camera, begin);
        global_selection.update(model, model != nullptr, behavior);
        return global_selection.size();
    } else {
        auto models = scene.get_models_in_region(camera, begin, end);
        global_selection.update_multiple(models, behavior);
        return global_selection.size();
    }
}

size_t ViewportUI::select_elements(
    bool persistent, Eigen::Vector2f begin, Eigen::Vector2f end, SelectionBehavior behavior)
{
    if (!persistent) return 0;

    auto& selection = get_viewer()->get_selection();
    auto& global_selection = selection.get_global();
    const auto mode = selection.get_selection_mode();

    auto& viewport = get();
    const auto& camera = viewport.get_camera();

    if (end.x() < begin.x()) std::swap(end.x(), begin.x());
    if (end.y() < begin.y()) std::swap(end.y(), begin.y());

    const auto ignore_backfacing = !selection.select_backfaces();

    size_t total_selected = 0;

    for (auto obj : global_selection.get_persistent().get_selection()) {
        auto model = dynamic_cast<MeshModelBase*>(obj);
        if (!model) continue;

        auto& model_selection = persistent ? model->get_selection().get_persistent()
                                           : model->get_selection().get_transient();

        if (model->get_selection().get_type() != mode) {
            model->get_selection().get_persistent().clear();
            model->get_selection().get_transient().clear();
            model->get_selection().set_type(mode);
        }

        if (end.x() - begin.x() < 2 && end.y() - begin.y() < 2) {
            int elem_id = -1;

            if (!persistent) return 0;
            if (mode == SelectionElementType::FACE) {
                model->get_facet_at(camera, begin, elem_id);
            } else if (mode == SelectionElementType::VERTEX) {
                model->get_vertex_at(camera, begin, 40.0f, elem_id);
            } else if (mode == SelectionElementType::EDGE) {
                model->get_edge_at(camera, begin, 40.0f, elem_id);
            }

            model_selection.update(elem_id, elem_id != -1, behavior);
            total_selected += model_selection.size();
        } else {
            std::unordered_set<int> result;

            if (mode == SelectionElementType::FACE) {
                result = model->get_facets_in_frustum(camera, begin, end, ignore_backfacing);
            } else if (mode == SelectionElementType::EDGE) {
                result = model->get_edges_in_frustum(camera, begin, end, ignore_backfacing);
            } else if (mode == SelectionElementType::VERTEX) {
                result = model->get_vertices_in_frustum(camera, begin, end, ignore_backfacing);
            }
            model_selection.update_multiple(result, behavior);
            total_selected += model_selection.size();
        }
    }
    return total_selected;
}

void ViewportUI::rotate_camera()
{
    const auto& io = ImGui::GetIO();
    auto& viewport = get();
    auto& cam = viewport.get_camera();

    Eigen::Vector2f p = screen_to_viewport(ImGui::GetMousePos());
    Eigen::Vector2f delta = get_viewer()->get_mouse_delta();
    Eigen::Vector2f d =
        Eigen::Vector2f(delta.x() / cam.get_window_width(), delta.y() / cam.get_window_height());

    if (cam.get_type() == Camera::Type::PERSPECTIVE || !m_ortho_interaction_2D) {
        Eigen::Vector2f angle = (d * 4.0f * float(cam.get_retina_scale()));
        if (io.KeyShift) angle *= 0.1f;
        std::swap(angle.x(), angle.y());
        angle.y() = -angle.y();

        switch (cam.get_rotation_mode()) {
        case Camera::RotationMode::TUMBLE: cam.rotate_tumble(angle.y(), angle.x()); break;
        case Camera::RotationMode::TURNTABLE: cam.rotate_turntable(angle.y(), angle.x()); break;
        case Camera::RotationMode::ARCBALL:
            cam.rotate_arcball(m_rotation_camera_pos_start,
                m_rotation_camera_up_start,
                Eigen::Vector2f(m_rotation_mouse_start.x(), m_rotation_mouse_start.y()),
                Eigen::Vector2f(p.x(), p.y()));
            break;
        }
    } else if (cam.get_type() == Camera::Type::ORTHOGRAPHIC) {
        auto v = cam.get_ortho_viewport();
        float dx = -std::abs(v.x() - v.y());
        float dy = -std::abs(v.w() - v.z());
        v += Eigen::Vector4f(d.x() * dx, d.x() * dx, d.y() * dy, d.y() * dy);
        cam.set_ortho_viewport(v);
    }
}

void ViewportUI::gizmo(const Selection<BaseObject*>& selection)
{
    std::vector<Model*> models;
    for (auto object : selection.get_selection()) {
        auto model = dynamic_cast<Model*>(object);
        if (model) models.push_back(model);
    }

    const SelectionElementType sel_mode = get_viewer()->get_selection().get_selection_mode();
    // Element transform only when one object selected & mesh mode
    if (sel_mode != SelectionElementType::OBJECT && models.size() != 1 &&
        !dynamic_cast<MeshModelBase*>(models.front()))
        return;

    auto& viewport = get();

    // Copy camera for imguizmo
    Camera cam = viewport.get_camera();

    /*
        Imguizmo at this version (ace44b6ddae5c9ee7e48d8e25589838ec40227fa)
        SetOrthographic does nothing but break rotation gizmo render
        For orthographic projection to work, only the far/near plane have to be adjusted
    */
    if (cam.get_type() == Camera::Type::ORTHOGRAPHIC) {
        //Near/far planes have to be symmetrical for imguizmo to
        // handle orthographic projection
        cam.set_planes(-1000.0f, 1000.0f);
    }

    auto V = cam.get_view();
    auto P = cam.get_perspective();


    /*
        Set up viewing rectangle for gizmo
    */
    if (models.size() == 1) {
        auto vt = models.front()->get_viewport_transform();
        auto obj_cam = cam.transformed(vt);

        if (vt.scale.x() != 1.0f || vt.scale.y() != 1.0f) {
            ImGuizmo::SetRect(m_canvas_pos.x() + (obj_cam.get_window_origin().x()),
                m_canvas_pos.y() + (cam.get_window_height() - obj_cam.get_window_origin().y() -
                                       obj_cam.get_window_height()),
                float(obj_cam.get_window_width()),
                float(obj_cam.get_window_height()));
        } else {
            ImGuizmo::SetRect(m_canvas_pos.x(),
                m_canvas_pos.y(),
                float(cam.get_window_width()),
                float(cam.get_window_height()));
        }
    } else {
        ImGuizmo::SetRect(m_canvas_pos.x(),
            m_canvas_pos.y(),
            float(cam.get_window_width()),
            float(cam.get_window_height()));
    }


    ImGuizmo::OPERATION op = ImGuizmo::BOUNDS;

    switch (get_viewer()->get_manipulation_mode()) {
    case Viewer::ManipulationMode::TRANSLATE: op = ImGuizmo::TRANSLATE; break;
    case Viewer::ManipulationMode::ROTATE: op = ImGuizmo::ROTATE; break;
    case Viewer::ManipulationMode::SCALE: op = ImGuizmo::SCALE; break;
    default: break;
    }

    AABB elem_sel_bounds;

    if (!m_gizmo_active && !ImGuizmo::IsUsing()) {
        Eigen::Vector3f avg_pos = Eigen::Vector3f::Zero();

        if (sel_mode == SelectionElementType::OBJECT) {
            for (auto model : models) {
                auto bounds = model->get_bounds();
                avg_pos += bounds.center();
            }
            avg_pos *= 1.0f / float(models.size());
        } else {
            auto mesh_model = reinterpret_cast<MeshModelBase*>(models.front());
            elem_sel_bounds = mesh_model->get_selection_bounds();
            avg_pos = ((elem_sel_bounds.min() + elem_sel_bounds.max()) * 0.5f);
        }

        m_gizmo_transform_start = Eigen::Translation3f(avg_pos);
        m_gizmo_transform = m_gizmo_transform_start;


    } else if (!m_gizmo_active && ImGuizmo::IsUsing()) {
        m_gizmo_active = true;
        m_gizmo_object_transforms.clear();

        // Save initial transforms
        for (auto model : models) {
            m_gizmo_object_transforms[model] = model->get_transform();
        }
    } else if (m_gizmo_active && !ImGuizmo::IsUsing()) {
        m_gizmo_active = false;
    }


    if (sel_mode == SelectionElementType::OBJECT) {
        AABB bounds;
        for (auto model : models) {
            auto model_bounds = model->get_bounds();
            bounds.extend(model_bounds);
        }

        Eigen::Vector3f bbox_center = bounds.center();
        Eigen::Matrix<float, 2, 3, Eigen::RowMajor> bb;
        bb << (bounds.min() - bbox_center).transpose(), (bounds.max()-bbox_center).transpose();

        ImGuizmo::Manipulate(V.data(),
            P.data(),
            op,
            ImGuizmo::WORLD,
            m_gizmo_transform.data(),
            nullptr,
            nullptr,
            (op == ImGuizmo::SCALE) ? bb.data() : nullptr,
            nullptr);

        if (m_gizmo_active) {
            // Set final transform without initial gizmo transform
            Eigen::Affine3f inv_start = m_gizmo_transform_start.inverse();

            const bool has_callback_post_transform =
                m_callbacks.has_callback<PostUpdateModelTransform>();
            std::vector<std::pair<const Model*, const Eigen::Affine3f>> collected_transforms;
            for (auto model : models) {
                const Eigen::Affine3f M =
                    m_gizmo_transform * inv_start * m_gizmo_object_transforms[model];

                const bool transform_changed =
                    !(M.matrix().array() == model->get_transform().matrix().array()).all();

                if (transform_changed) {
                    model->set_transform(M);
                    if (has_callback_post_transform) {
                        collected_transforms.emplace_back(model, M);
                    }
                }
            }
            if (has_callback_post_transform && collected_transforms.size() > 0) {
                m_callbacks.call<PostUpdateModelTransform>(collected_transforms);
            }
        }

    } else if (models.size() == 1) {
        auto* mesh_model = reinterpret_cast<MeshModelBase*>(models.front());

        if (mesh_model->get_selection().get_persistent().size() > 0) {
            // Save previous gizmo transform so we can apply per frame delta later
            const auto prev_transform = m_gizmo_transform;

            ImGuizmo::Manipulate(V.data(),
                P.data(),
                op,
                ImGuizmo::WORLD,
                m_gizmo_transform.data(),
                nullptr,
                nullptr,
                nullptr,
                nullptr);

            if (m_gizmo_active) {
                const auto model = m_gizmo_object_transforms[models.front()];
                const Eigen::Affine3f adjusted_delta =
                    (model.inverse() * m_gizmo_transform * prev_transform.inverse() * model);

                const bool transform_changed =
                    !(adjusted_delta.matrix().array() == Eigen::Affine3f::Identity().matrix().array()).all();

                if (transform_changed) {
                    mesh_model->transform_selection(adjusted_delta);
                }

            }
        }
    }
}

Eigen::Vector2f& ViewportUI::get_last_mouse_click_pos(int key)
{
    return m_last_mouse_click[key];
}

Eigen::Vector2f& ViewportUI::get_last_mouse_pos()
{
    return m_last_mouse_pos;
}

bool ViewportUI::hovered() const
{
    return m_hovered;
}

void ViewportUI::reset_viewport_ui_counter()
{
    ViewportUI::m_viewport_ui_counter = 0;
}

Eigen::Vector2f ViewportUI::get_viewport_screen_position() const
{
    return m_canvas_pos;
}

void ViewportUI::enable_selection(bool value)
{
    m_selection_enabled = value;
}

bool ViewportUI::is_selection_enabled() const
{
    return m_selection_enabled;
}

} // namespace ui
} // namespace lagrange

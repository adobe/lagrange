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

#include <lagrange/Logger.h>
#include <lagrange/ui/components/CameraComponents.h>
#include <lagrange/ui/components/Common.h>
#include <lagrange/ui/components/Viewport.h>
#include <lagrange/ui/imgui/UIWidget.h>
#include <lagrange/ui/imgui/buttons.h>
#include <lagrange/ui/panels/ViewportPanel.h>
#include <lagrange/ui/systems/render_viewports.h>
#include <lagrange/ui/systems/update_gizmo.h>
#include <lagrange/ui/systems/update_scene_bounds.h>
#include <lagrange/ui/types/Camera.h>
#include <lagrange/ui/types/Systems.h>
#include <lagrange/ui/utils/uipanel.h>
#include <lagrange/ui/utils/bounds.h>
#include <lagrange/ui/utils/input.h>
#include <lagrange/ui/utils/layer.h>
#include <lagrange/ui/utils/selection.h>
#include <lagrange/ui/utils/viewport.h>

#include <IconsFontAwesome5.h>
#include <imgui.h>

namespace lagrange {
namespace ui {

namespace {


struct NoPadding
{
    NoPadding() { ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); }
    ~NoPadding() { ImGui::PopStyleVar(); }
};


void draw_imgui_gl_texture(GLuint tex_id, int w, int h)
{
    const ImTextureID texID = reinterpret_cast<void*>((long long int)(tex_id));

    const ImVec2 uv0 = ImVec2(0, 1);
    const ImVec2 uv1 = ImVec2(1, 0);

    ImGui::Image(
        texID,
        ImVec2(float(w), float(h)),
        uv0,
        uv1,
        ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
        ImVec4(1.0f, 1.0f, 1.0f, 0.5f));
}

void separator()
{
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(p.x, p.y),
        ImVec2(p.x, p.y + ImGui::GetTextLineHeightWithSpacing()),
        ImColor(ImGui::Spectrum::GRAY300),
        1.0f);
    ImGui::Dummy(ImVec2(2.5f, 0));
    ImGui::SameLine();
}
void draw_texture_widget(Texture& tex)
{
    auto w = tex.get_width();
    auto h = tex.get_height();
    if (w > 0 && h > 0) {
        const auto display_w = static_cast<int>(ImGui::GetContentRegionAvail().y - 20.0f);
        const auto display_h = static_cast<int>((float(h) / w) * display_w);

        const auto& p = tex.get_params();

        if (p.type == GL_TEXTURE_CUBE_MAP) {
            ImGui::Text("Cubemap texture widget not implemented");
        } else {
            draw_imgui_gl_texture(tex.get_id(), display_w, display_h);
        }


        ImGui::Text("Width: %d, Height: %d", w, h);


        ImGui::Text("Type: ");
        ImGui::SameLine();
        switch (p.type) {
        case GL_TEXTURE_1D: ImGui::Text("GL_TEXTURE_1D"); break;
        case GL_TEXTURE_2D: ImGui::Text("GL_TEXTURE_2D"); break;
        case GL_TEXTURE_3D: ImGui::Text("GL_TEXTURE_3D"); break;
        case GL_TEXTURE_2D_MULTISAMPLE: ImGui::Text("GL_TEXTURE_2D_MULTISAMPLE"); break;
        case GL_TEXTURE_CUBE_MAP: ImGui::Text("GL_TEXTURE_CUBE_MAP"); break;
        default: ImGui::Text("%x", p.type); break;
        }

        ImGui::Text("Format: ");
        ImGui::SameLine();
        switch (p.format) {
        case GL_RGB: ImGui::Text("GL_RGB"); break;
        case GL_RED: ImGui::Text("GL_RED"); break;
        case GL_RGBA: ImGui::Text("GL_RGBA"); break;
        case GL_DEPTH_COMPONENT: ImGui::Text("GL_DEPTH_COMPONENT"); break;
        default: ImGui::Text("%x", p.format); break;
        }

        ImGui::Text("Internal Format: ");
        ImGui::SameLine();
        switch (p.format) {
        case GL_R8: ImGui::Text("GL_R8"); break;
        case GL_RGB: ImGui::Text("GL_RGB"); break;
        case GL_RGBA: ImGui::Text("GL_RGBA"); break;
        case GL_DEPTH_COMPONENT24: ImGui::Text("GL_DEPTH_COMPONENT24"); break;
        case GL_RGB16F: ImGui::Text("GL_RGB16F"); break;
        default: ImGui::Text("%x", p.format); break;
        }

        ImGui::Text("Generate Mipmap: %s", p.generate_mipmap ? "True" : "False");
        ImGui::Text("sRGB: %s", p.sRGB ? "True" : "False");

        auto label = string_format("Texture ID: {}", tex.get_id());
        ImGui::Text("Texture ID: %u", tex.get_id());
    }
}


void draw_render_pass_popup(Registry& registry, ViewportPanel& data)
{
#ifdef LGUI_RENDERER_DEPRECATED
    if (!ImGui::BeginPopupContextItem(render_pass_popup_name(registry, data).c_str())) return;


    if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();

    ImGui::Dummy(ImVec2(400, 0));


    auto& viewport = registry.get<ViewportComponent>(data.viewport);


    auto& renderer = *viewport.renderer;

    auto& all_passes = renderer.get_pipeline().get_passes();


    auto display_pass = [&](RenderPassBase* pass) {
        const auto& display_name = pass->get_name();
        bool enabled = is_render_pass_enabled(viewport, pass);

        if (ImGui::Checkbox(display_name.c_str(), &enabled)) {
            enable_render_pass(viewport, pass, enabled);
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
#endif
}

void draw_framebuffer_popup(Registry& registry, ViewportPanel& data)
{
    ImGui::PushID("viewportopt");
    if (ImGui::Button("Viewport " ICON_FA_CARET_DOWN)) {
        ImGui::OpenPopup("viewportopt");
    }

    ImGui::SameLine();

    if (ImGui::BeginPopupContextItem("viewportopt")) {
        ImGui::Dummy(ImVec2(400, 0));

        registry.view<ViewportComponent>().each([&](Entity e, ViewportComponent& v) {
            ImGui::PushID(int(e));


            bool sel = data.viewport == e;
            if (ImGui::Checkbox(ui::get_name(registry, e).c_str(), &sel)) {
                if (sel) data.viewport = e;
            }


            ImGui::Indent();

            if (ImGui::CollapsingHeader("Viewport Options")) {
                ImGui::InputInt("Width", &v.width);
                ImGui::InputInt("Height", &v.height);
                ImGui::Checkbox("Auto near/far", &v.auto_nearfar);
            }

            if (ImGui::CollapsingHeader("Layer visibility")) {
                for (auto i = 0; i < get_max_layers(); i++) {
                    const auto& name = get_layer_name(registry, i);
                    if (name.length() == 0) continue;

                    bool value = v.visible_layers.test(i);
                    if (ImGui::Checkbox(name.c_str(), &value)) {
                        v.visible_layers.flip(i);
                    }
                }
            }

            if (ImGui::CollapsingHeader("Post process effects")) {
                for (auto& it : v.post_process_effects) {
                    ImGui::Text("%s", it.first.c_str());
                }
            }


            if (v.fbo) {
                auto max_colors = FrameBuffer::get_max_color_attachments();

                if (ImGui::CollapsingHeader("Color attachments")) {
                    for (auto i = 0; i < max_colors; i++) {
                        ImGui::Text("[Color %d] Attachment", i);
                        auto ptr = v.fbo->get_color_attachement(i);
                        if (ptr) {
                            draw_texture_widget(*ptr);
                        }
                    }
                }

                if (ImGui::CollapsingHeader("Depth attachment")) {
                    auto ptr = v.fbo->get_depth_attachment();
                    if (ptr) {
                        draw_texture_widget(*ptr);
                    }
                }
            }


            // ImGui::Separator();

            // Layers


            /*  for (auto& p : v.post_process_effects) {
                  ImGui::Text("Post process effect");
                  // show_widget(registry, e, entt::resolve(entt::type_id<NEWMaterial>()));
              }*/


            ImGui::Unindent();


            ImGui::PopID();
        });


        ImGui::EndPopup();
    }

    ImGui::PopID();
}

void draw_viewport_toolbar(Registry& registry, ViewportPanel& data)
{
    auto& keys = get_keybinds(registry);
    auto& viewport = registry.get<ViewportComponent>(data.viewport);
    auto& cam = get_viewport_camera(registry, viewport);


    // Add padding, as window padding is disabled
    ImGui::Dummy(ImVec2(5, 1));
    ImGui::Dummy(ImVec2(1, 5));
    ImGui::SameLine();

    /*
       Render appearence
   */
#ifdef LGUI_RENDERER_DEPRECATED
    if (viewport.renderer) {
        const bool pbr = is_render_pass_enabled_tag(viewport, "pbr");
        const bool edges = is_render_pass_enabled(viewport, "Edges");
        const bool vertices = is_render_pass_enabled(viewport, "Vertices");
        const bool normal = is_render_pass_enabled_tag(viewport, "normal");
        const bool bbox = is_render_pass_enabled_tag(viewport, "boundingbox");

        if (button(pbr, ICON_FA_LIGHTBULB, "Physically Based Render")) {
            enable_render_pass_tag(viewport, "pbr", !pbr);
        }
        ImGui::SameLine();

        if (button(edges, ICON_FA_ARROWS_ALT_H, "Edges")) {
            enable_render_pass(viewport, "Edges", !edges);
        }
        ImGui::SameLine();

        if (button(vertices, ICON_FA_CIRCLE, "Vertices")) {
            enable_render_pass(viewport, "Vertices", !vertices);
        }
        ImGui::SameLine();

        if (button(normal, ICON_FA_EXTERNAL_LINK_ALT, "Normals")) {
            enable_render_pass_tag(viewport, "normal", !normal);
        }
        ImGui::SameLine();

        if (button(bbox, ICON_FA_VECTOR_SQUARE, "Bounding Box")) {
            enable_render_pass_tag(viewport, "boundingbox", !bbox);
        }
        ImGui::SameLine();
    }
#endif

/*
   Normals
*/
#ifdef LGUI_RENDERER_DEPRECATED
    if (viewport.renderer) {
        auto& renderer = *viewport.renderer;
        ImGui::PushID("normals");
        if (ImGui::Button(ICON_FA_CARET_DOWN)) {
            ImGui::OpenPopup("Normals");
        }

        ImGui::SameLine();

        if (ImGui::BeginPopupContextItem("Normals")) {
            auto* pass = renderer.get_pipeline().get_pass(default_render_pass_name<NormalsPass>());
            auto& opt = pass->get_options();

            float w = 250.0f;
            bool face = opt["Face"].get<bool>("Enabled");
            if (ImGui::Selectable(
                    ICON_FA_EXTERNAL_LINK_ALT " Face Normals",
                    face,
                    0,
                    ImVec2(w, 0))) {
                opt["Face"].set<bool>("Enabled", !face);
            }

            bool corner = opt["Corner Vertex"].get<bool>("Enabled");
            if (ImGui::Selectable(
                    ICON_FA_EXTERNAL_LINK_SQUARE_ALT "   Vertex Corner Normals",
                    corner,
                    0,
                    ImVec2(w, 0))) {
                opt["Corner Vertex"].set<bool>("Enabled", !corner);
            }

            bool pervertex = opt["Per-Vertex"].get<bool>("Enabled");
            if (ImGui::Selectable(
                    ICON_FA_ASTERISK "  Per-Vertex Normals",
                    pervertex,
                    0,
                    ImVec2(w, 0))) {
                opt["Per-Vertex"].set<bool>("Enabled", !pervertex);
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();

        separator();
    }


    /*
        Post processing
    */
    if (viewport.renderer) {
        const bool outline = is_render_pass_enabled_tag(viewport, "outline");
        const bool fxaa = is_render_pass_enabled(viewport, default_render_pass_name<FXAAPass>());

        if (button(outline, ICON_FA_OBJECT_GROUP, "Selection Outline")) {
            enable_render_pass_tag(viewport, "outline", !outline);
        }
        ImGui::SameLine();

        if (button(fxaa, "AA", "FXAA")) {
            enable_render_pass(viewport, default_render_pass_name<FXAAPass>(), !fxaa);
        }
        ImGui::SameLine();

        separator();
    }


    /*
        Render pass details
    */
    if (viewport.renderer) {
        // Get unique name
        if (ImGui::Button("Render passes " ICON_FA_CARET_DOWN)) {
            ImGui::OpenPopup(render_pass_popup_name(registry, data).c_str());
        }
        ImGui::SameLine();
        separator();
    }

#endif


    /////////////////

    /*
        Camera
    */
    if (registry.valid(viewport.camera_reference) &&
        registry.has<CameraController>(viewport.camera_reference)) {
        auto& controller = registry.get<CameraController>(viewport.camera_reference);

        // Camera modes
        {
            const bool perspective = (cam.get_type() == Camera::Type::PERSPECTIVE);

            if (button_icon(perspective, ICON_FA_LESS_THAN, "Perspective")) {
                if (!perspective)
                    cam.set_type(Camera::Type::PERSPECTIVE);
                else
                    cam.set_type(Camera::Type::ORTHOGRAPHIC);
            }
            ImGui::SameLine();

            if (button_icon(!perspective, ICON_FA_BARS, "Orthographic")) {
                if (!perspective)
                    cam.set_type(Camera::Type::PERSPECTIVE);
                else
                    cam.set_type(Camera::Type::ORTHOGRAPHIC);
            }
            ImGui::SameLine();

            if (!perspective) {
                if (button_icon(
                        controller.ortho_interaction_2D,
                        "2D",
                        "2D Orthographic Camera Interaction")) {
                    controller.ortho_interaction_2D = !controller.ortho_interaction_2D;
                }
                ImGui::SameLine();
                if (button_icon(
                        !controller.ortho_interaction_2D,
                        "3D",
                        "3D Orthographic Camera Interaction")) {
                    controller.ortho_interaction_2D = !controller.ortho_interaction_2D;
                }
                ImGui::SameLine();
            }
        }

        // Camera direction
        {
            if (button_icon(
                    cam.is_orthogonal_direction(Camera::Dir::TOP),
                    "Top",
                    "Top-Down View")) {
                cam.set_orthogonal_direction(Camera::Dir::TOP);
            }
            ImGui::SameLine();

            if (button_icon(cam.is_orthogonal_direction(Camera::Dir::BOTTOM), "Down", "Up View")) {
                cam.set_orthogonal_direction(Camera::Dir::BOTTOM);
            }
            ImGui::SameLine();

            if (button_icon(
                    cam.is_orthogonal_direction(Camera::Dir::FRONT),
                    "Front",
                    "Front View")) {
                cam.set_orthogonal_direction(Camera::Dir::FRONT);
            }
            ImGui::SameLine();

            if (button_icon(cam.is_orthogonal_direction(Camera::Dir::LEFT), "Left", "Left View")) {
                cam.set_orthogonal_direction(Camera::Dir::LEFT);
            }
            ImGui::SameLine();

            if (button_icon(cam.is_orthogonal_direction(Camera::Dir::BACK), "Back", "Back View")) {
                cam.set_orthogonal_direction(Camera::Dir::BACK);
            }
            ImGui::SameLine();

            if (button_icon(
                    cam.is_orthogonal_direction(Camera::Dir::RIGHT),
                    "Right",
                    "Right View")) {
                cam.set_orthogonal_direction(Camera::Dir::RIGHT);
            }
            ImGui::SameLine();
        }


        // Multi viewport
        {
            if (button_icon(false, ICON_FA_SHARE_SQUARE, "Instance camera to other viewports")) {
                instance_camera_to_viewports(registry, viewport.camera_reference);
            }
            ImGui::SameLine();

            if (button_icon(false, ICON_FA_SHARE, "Copy camera to other viewports")) {
                copy_camera_to_viewports(registry, viewport.camera_reference);
            }
            ImGui::SameLine();
        }


        {
            bool fit = false;
            bool focus = false;
            if (button_icon(
                    false,
                    ICON_FA_COMPRESS_ARROWS_ALT,
                    "Focus On Selection",
                    "global.camera.center_on_selection",
                    &keys)) {
                focus = true;
            }
            ImGui::SameLine();

            if (button_icon(
                    false,
                    ICON_FA_EXPAND,
                    "Zoom To Fit Selection",
                    "global.camera.zoom_to_fit",
                    &keys)) {
                fit = true;
                focus = true;
            }

            if ((fit || focus) && registry.valid(viewport.camera_reference)) {
                camera_focus_and_fit(
                    registry,
                    viewport.camera_reference,
                    focus,
                    fit,
                    1.0f,
                    [](Registry& r, Entity e) { return is_selected(r, e); });
            }

            ImGui::SameLine();
        }


        if (button_icon(!controller.fov_zoom, ICON_FA_ARROWS_ALT_V, "Dolly")) {
            controller.fov_zoom = !controller.fov_zoom;
        }

        ImGui::SameLine();

        separator();

        if (button_icon(controller.auto_nearfar, ICON_FA_COMPRESS, "Automatic Clipping Planes")) {
            controller.auto_nearfar = !controller.auto_nearfar;
        }

        ImGui::SameLine();

        separator();
    }


#ifdef LGUI_RENDERER_DEPRECATED
    /*
        Ground rendering
    */
    if (viewport.renderer) {
        auto& renderer = *viewport.renderer;
        bool ground_pass = is_render_pass_enabled(viewport, default_render_pass_name<GroundPass>());
        if (button(ground_pass, ICON_FA_TH, "Toggle Ground")) {
            enable_render_pass(viewport, default_render_pass_name<GroundPass>(), !ground_pass);
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

        separator();
    }
#endif

    draw_framebuffer_popup(registry, data);

    separator();

    ImGui::Dummy(ImVec2(0, 0));
}


void draw_viewport_texture(ViewportComponent& viewport)
{
    Texture* texture = nullptr;
    if (viewport.fbo->has_color_attachment()) {
        texture = viewport.fbo->get_color_attachement(0).get();
    } else if (viewport.fbo->has_depth_attachement()) {
        texture = viewport.fbo->get_depth_attachment().get();
    }
    if (!texture) {
        ImGui::Text("No framebuffer attachement");
        return;
    }

    draw_imgui_gl_texture(texture->get_id(), viewport.width, viewport.height);
}


void viewport_panel_system(Registry& registry, Entity e)
{
    ViewportPanel& data = registry.get<ViewportPanel>(e);
    auto& input = get_input(registry);
    auto& viewport = registry.get<ViewportComponent>(data.viewport);


    // Update FocusedViewportPanel
    if (registry.get<UIPanel>(e).is_focused) {
        registry.ctx<FocusedViewportPanel>().viewport_panel = e;
    }

    Camera& cam = get_viewport_camera(registry, viewport);

    ImGui::PushID(int(e));

    // Draw render pass popup
    draw_render_pass_popup(registry, data);

    // Draw toolbar
    if (data.show_viewport_toolbar) {
        draw_viewport_toolbar(registry, data);
    }

    {
        NoPadding _;
        // Look at current imgui state
        data.canvas_origin = ImGui::GetCursorScreenPos();
        data.available_width = std::max(16, static_cast<int>(ImGui::GetContentRegionAvail().x));
        data.available_height =
            std::max(16, static_cast<int>(ImGui::GetContentRegionAvail().y - 2));

        // Update viewport dimensions
        viewport.width = data.available_width;
        viewport.height = data.available_height;

        // Update camera dimensions (needed in camera controller)
        // TODO: camera controller shouldn't care about pixels
        cam.set_window_dimensions(float(viewport.width), float(viewport.height));

        // Draw the canvas/texture
        draw_viewport_texture(viewport);
    }

    // Compute mouse state
    data.hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootWindow) &&
                   data.is_over_viewport(input.mouse_position);
    data.mouse_in_canvas = data.screen_to_viewport(input.mouse_position);


    if (!registry.valid(viewport.camera_reference) ||
        !registry.has<CameraController>(viewport.camera_reference)) {
        ImGui::PopID();
        return;
    }

    CameraController& controller = registry.get<CameraController>(viewport.camera_reference);
    auto& keys = *input.keybinds;

    // If this viewport has focus, set draw list for gizmo here
    if (e == registry.ctx<FocusedViewportPanel>().viewport_panel) {
        gizmo_system_set_draw_list();
    }

    // Check for gizmo system being used by Tools
    bool hovered_no_gizmo = data.hovered && !gizmo_system_is_using() && !gizmo_system_is_over();

    if (hovered_no_gizmo) {
        if (keys.is_pressed("viewport.camera.rotate")) {
            if (!controller.rotation_active) {
                controller.rotation_mouse_start = data.mouse_in_canvas;
                controller.rotation_camera_pos_start = cam.get_position();
                controller.rotation_camera_up_start = cam.get_up();
            }
            controller.rotation_active = true;
        }

        // Zoom using mousewheel
        if (input.mouse_wheel != 0.0f) {
            controller.dolly_active = true;
            controller.dolly_delta = static_cast<float>(0.15f * input.mouse_wheel);
        } else {
            controller.dolly_active = false;
        }


        // Zoom using dolly keybind
        if (keys.is_down("viewport.camera.dolly")) {
            controller.dolly_active = true;
            controller.dolly_delta = float(input.mouse_delta.x()) / cam.get_window_width();
        }


        if (keys.is_down("viewport.camera.pan")) {
            controller.pan_active = true;
        }
    }


    if (keys.is_released("viewport.camera.rotate")) {
        controller.rotation_active = false;
    }

    if (keys.is_released("viewport.camera.dolly")) {
        controller.dolly_active = false;
    }


    if (keys.is_released("viewport.camera.pan")) {
        controller.pan_active = false;
    }

    controller.mouse_current = data.mouse_in_canvas;
    controller.mouse_delta = input.mouse_delta;


    ImGui::PopID();
}

} // namespace


bool ViewportPanel::is_over_viewport(const Eigen::Vector2f& pos) const
{
    return pos.x() > canvas_origin.x() && pos.y() > canvas_origin.y() &&
           pos.x() < canvas_origin.x() + available_width &&
           pos.y() < canvas_origin.y() + available_height;
}

Eigen::Vector2f ViewportPanel::screen_to_viewport(const Eigen::Vector2f& pos) const
{
    return {pos.x() - canvas_origin.x(), available_height - (pos.y() - canvas_origin.y())};
}


Eigen::Vector2f ViewportPanel::viewport_to_screen(const Eigen::Vector2f& viewport_pos) const
{
    return {
        viewport_pos.x() + canvas_origin.x(),
        canvas_origin.y() + available_height - viewport_pos.y()};
}

Entity add_viewport_panel(Registry& r, const std::string& name /*= "Viewport"*/, Entity viewport)
{
    auto e = add_panel(r, name, viewport_panel_system);
    ViewportPanel vw;
    vw.viewport = viewport;
    r.emplace<ViewportPanel>(e, std::move(vw));
    return e;
}

} // namespace ui
} // namespace lagrange

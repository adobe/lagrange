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
#include <lagrange/ui/default_tools.h>
#include <lagrange/ui/types/Tools.h>

#include <lagrange/ui/components/Common.h>
#include <lagrange/ui/components/ObjectIDViewport.h>
#include <lagrange/ui/components/SelectionContext.h>
#include <lagrange/ui/components/Viewport.h>
#include <lagrange/ui/systems/render_viewports.h>
#include <lagrange/ui/systems/update_gizmo.h>
#include <lagrange/ui/types/Camera.h>
#include <lagrange/ui/types/Material.h>
#include <lagrange/ui/utils/input.h>
#include <lagrange/ui/utils/selection.h>
#include <lagrange/ui/utils/viewport.h>


#include <lagrange/ui/panels/ViewportPanel.h>


#include <ImGuizmo.h>
#include <imgui.h>

namespace lagrange {
namespace ui {

void transform_tool_impl(Registry& registry, GizmoMode mode)
{
    auto& viewport_panel =
        registry.get<ViewportPanel>(registry.ctx<FocusedViewportPanel>().viewport_panel);
    auto& viewport = registry.get<ViewportComponent>(viewport_panel.viewport);
    auto& camera = get_viewport_camera(registry, viewport);

    gizmo_system(registry, camera, viewport_panel.canvas_origin, mode);

    const bool no_guizmo = !ImGuizmo::IsUsing() && !ImGuizmo::IsOver();

    // Fallback to selection
    if (no_guizmo) {
        registry.ctx<Tools>().run<SelectToolTag, ElementObject>(registry);
        return;
    }
}

void select(Registry& r, const std::function<void(Registry&)>& selection_system)
{
    const auto& input = get_input(r);
    const auto& keys = *input.keybinds;
    const auto& panel = get_focused_viewport_panel(r);

    auto& tools = r.ctx<Tools>();

    auto& sel_ctx = get_selection_context(r);

    // Update some members regardless of selection being active
    sel_ctx.element_type = tools.current_element_type();
    sel_ctx.screen_position = input.mouse_position;
    sel_ctx.behavior = selection_behavior(keys);


    // auto hovered_viewport = get_hovered_viewport(r);
    if (!are_selection_keys_down(keys) && !are_selection_keys_released(keys)) {
        sel_ctx.active_viewport = get_hovered_viewport_entity(r);
    }

    // No hovered or active viewport
    if (!r.valid(sel_ctx.active_viewport)) {
        return;
    }

    auto& viewport_component = r.get<ViewportComponent>(sel_ctx.active_viewport);

    sel_ctx.viewport_position =
        panel.screen_to_viewport(sel_ctx.screen_position)
            .cast<int>()
            .cwiseMax(Eigen::Vector2i::Zero())
            .cwiseMin(Eigen::Vector2i(viewport_component.width, viewport_component.height));

    const auto& cam = viewport_component.computed_camera;
    const auto ray = cam.cast_ray(sel_ctx.viewport_position.cast<float>());
    sel_ctx.ray_dir = ray.dir;
    sel_ctx.ray_origin = ray.origin;
    sel_ctx.camera = cam;


    // Selection start, create a global marquee rectangle
    if (!sel_ctx.active && are_selection_keys_pressed(keys)) {
        sel_ctx.active = true;
        sel_ctx.marquee_active = false;
        sel_ctx.screen_begin = sel_ctx.screen_position;
        sel_ctx.screen_end = sel_ctx.screen_position;
        sel_ctx.viewport_min = sel_ctx.viewport_position;
        sel_ctx.viewport_max = sel_ctx.viewport_position + Eigen::Vector2i::Ones();
    }

    // Update end position of the rectangle
    if (are_selection_keys_down(keys)) {
        sel_ctx.screen_end = sel_ctx.screen_position;

        const Eigen::Vector2i begin_viewport =
            panel.screen_to_viewport(sel_ctx.screen_begin)
                .cast<int>()
                .cwiseMax(Eigen::Vector2i::Zero())
                .cwiseMin(Eigen::Vector2i(viewport_component.width, viewport_component.height));

        const Eigen::Vector2i end_viewport =
            panel.screen_to_viewport(sel_ctx.screen_end)
                .cast<int>()
                .cwiseMax(Eigen::Vector2i::Zero())
                .cwiseMin(Eigen::Vector2i(viewport_component.width, viewport_component.height));

        sel_ctx.viewport_min = begin_viewport.cwiseMin(end_viewport);
        sel_ctx.viewport_max = begin_viewport.cwiseMax(end_viewport) + Eigen::Vector2i::Ones();

        // Marquee is active only if pixel area is greater than 1
        sel_ctx.marquee_active = (sel_ctx.viewport_max.x() - sel_ctx.viewport_min.x() > 1) ||
                                 (sel_ctx.viewport_max.y() - sel_ctx.viewport_min.y() > 1);
    }


    {
        const auto radius = Eigen::Vector2f::Constant(sel_ctx.neighbourhood_frustum_radius).eval();

        const Eigen::Vector2i neighbourhood_begin_viewport =
            panel.screen_to_viewport(sel_ctx.screen_position - radius)
                .cast<int>()
                .cwiseMax(Eigen::Vector2i::Zero())
                .cwiseMin(Eigen::Vector2i(viewport_component.width, viewport_component.height));

        const Eigen::Vector2i neighbourhood_end_viewport =
            panel.screen_to_viewport(sel_ctx.screen_position + radius)
                .cast<int>()
                .cwiseMax(Eigen::Vector2i::Zero())
                .cwiseMin(Eigen::Vector2i(viewport_component.width, viewport_component.height));

        const auto viewport_min =
            neighbourhood_begin_viewport.cwiseMin(neighbourhood_end_viewport).eval();
        const auto viewport_max =
            (neighbourhood_begin_viewport.cwiseMax(neighbourhood_end_viewport) +
             Eigen::Vector2i::Ones())
                .eval();

        sel_ctx.neighbourhood_frustum =
            cam.get_frustum(viewport_min.cast<float>(), viewport_max.cast<float>());
    }
    {
        const Eigen::Vector2i begin_onepx = sel_ctx.viewport_position;
        const Eigen::Vector2i end_onepx = begin_onepx + Eigen::Vector2i::Ones();
        sel_ctx.onepx_frustum = cam.get_frustum(begin_onepx.cast<float>(), end_onepx.cast<float>());
    }

    if (sel_ctx.marquee_active) {
        // Update frustum
        sel_ctx.frustum =
            cam.get_frustum(sel_ctx.viewport_min.cast<float>(), sel_ctx.viewport_max.cast<float>());

        // Draw selection rectangle
        const ImColor bg(1.0f, 1.0f, 1.0f, 0.05f);
        const ImColor border(1.0f, 1.0f, 1.0f, 0.25f);
        ImGui::GetForegroundDrawList()->AddRectFilled(sel_ctx.screen_begin, sel_ctx.screen_end, bg);
        ImGui::GetForegroundDrawList()->AddRect(sel_ctx.screen_begin, sel_ctx.screen_end, border);
    }

    // Resolve selection and remove the marquee rectangle
    if (are_selection_keys_released(keys)) {
        // Run system that converts from hover to selected
        selection_system(r);

        sel_ctx.active = false;
        sel_ctx.marquee_active = false;
    }
}

void select_object(Registry& r)
{
    const auto& sel_ctx = get_selection_context(r);
    SelectionBehavior behavior = sel_ctx.behavior;

    // Deselect all objects if needed
    if (behavior == SelectionBehavior::SET) {
        deselect_all(r);
        behavior = SelectionBehavior::ADD;
    }

    // Convert hovered to selected
    auto v = r.view<Hovered>();
    for (auto e : v) {
        ui::set_selected(r, e, behavior);
    }
}


void select_element(Registry& r) {}


void register_default_tools(Tools& tools)
{
    /*
        Register Element types
    */
    register_tool_type<ElementObject>("Object", ICON_FA_CUBES, "global.selection_mode.object");
    register_tool_type<ElementFace>("Face", ICON_FA_CUBE, "global.selection_mode.face");
    register_tool_type<ElementEdge>("Edge", ICON_FA_ARROWS_ALT_H, "global.selection_mode.edge");
    register_tool_type<ElementVertex>("Vertex", ICON_FA_CIRCLE, "global.selection_mode.vertex");

    /*
        Register Tool types
    */
    register_tool_type<SelectToolTag>(
        "Select",
        ICON_FA_VECTOR_SQUARE,
        "global.manipulation_mode.select");

    register_tool_type<TranslateToolTag>(
        "Translate",
        ICON_FA_ARROWS_ALT,
        "global.manipulation_mode.translate");

    register_tool_type<RotateToolTag>("Rotate", ICON_FA_REDO, "global.manipulation_mode.rotate");

    register_tool_type<ScaleToolTag>(
        "Scale",
        ICON_FA_COMPRESS_ARROWS_ALT,
        "global.manipulation_mode.scale");


    /*
        Register Tool X Element combination as
    */
    tools.register_tool<SelectToolTag, ElementObject>(
        [](Registry& r) { select(r, select_object); });

    tools.register_tool<TranslateToolTag, ElementObject>(
        [](Registry& r) { transform_tool_impl(r, GizmoMode::TRANSLATE); });
    tools.register_tool<RotateToolTag, ElementObject>(
        [](Registry& r) { transform_tool_impl(r, GizmoMode::ROTATE); });
    tools.register_tool<ScaleToolTag, ElementObject>(
        [](Registry& r) { transform_tool_impl(r, GizmoMode::SCALE); });


    tools.register_tool<SelectToolTag, ElementFace>([](Registry& r) { select(r, select_element); });

    tools.register_tool<SelectToolTag, ElementEdge>([](Registry& r) { select(r, select_element); });

    tools.register_tool<SelectToolTag, ElementVertex>(
        [](Registry& r) { select(r, select_element); });

    /*tools.register_tool<TranslateToolTag, ElementFace>(
        [](Registry& r) {
            transform_tool_impl(r, GizmoMode::TRANSLATE);
        });
    tools.register_tool<RotateToolTag, ElementFace>(
        [](Registry& r) {
            transform_tool_impl(r, GizmoMode::ROTATE);
        });
    tools.register_tool<ScaleToolTag, ElementFace>(
        [](Registry& r) {
            transform_tool_impl(r, GizmoMode::SCALE);
        });
*/


    /*tools.register_tool<SelectToolTag, ElementEdge>(select_object);
    tools.register_tool<SelectToolTag, ElementVertex>(select_object);*/


    tools.set_current_element_type(entt::resolve<ElementObject>().id());
    tools.set_current_tool_type(entt::resolve<SelectToolTag>().id());
}


} // namespace ui
} // namespace lagrange
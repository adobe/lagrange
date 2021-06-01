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


#include <lagrange/ui/components/Bounds.h>
#include <lagrange/ui/components/Selection.h>
#include <lagrange/ui/components/Transform.h>
#include <lagrange/ui/systems/update_gizmo.h>

#include <lagrange/Logger.h>
#include <lagrange/utils/la_assert.h>

///IMGUIZMO INCLUDE ORDER
#include <imgui.h>
////
#ifndef NULL
#define NULL nullptr
#endif
#include <ImGuizmo.h>
////
namespace lagrange {
namespace ui {


void gizmo_system(
    Registry& registry,
    const Camera& camera,
    const Eigen::Vector2f& canvas_pos,
    GizmoMode mode)
{
    // Gizmo state
    auto& ctx = registry.ctx_or_set<GizmoContext>();

    // Only transform selected entities
    auto view = registry.view<Selected, Transform>();


    // Make a local copy of camera
    auto cam = camera;

    //Adjust near/far planes
    if (cam.get_type() == Camera::Type::ORTHOGRAPHIC) {
        //Near/far planes have to be symmetrical for imguizmo to
        // handle orthographic projection
        cam.set_planes(-1000.0f, 1000.0f);
    }

    const auto V = cam.get_view();
    const auto P = cam.get_perspective();

    // Adjust viewport
    ImGuizmo::SetRect(
        canvas_pos.x(),
        canvas_pos.y(),
        float(cam.get_window_width()),
        float(cam.get_window_height()));

    // Count selected objects
    const auto selected_count =
        std::count_if(view.begin(), view.end(), [&](Entity e) { return 1; });

    if (selected_count == 0) return;

    // Adjust rectangle if only a single object is selected with scaled viewport transform
    if (selected_count == 1) {
        auto it = std::find_if(view.begin(), view.end(), [&](Entity e) { return 1; });

        const auto& vt = view.get<Transform>(*it).viewport;
        if (vt.scale.x() != 1.0f || vt.scale.y() != 1.0f) {
            auto obj_cam = cam.transformed(vt);

            ImGuizmo::SetRect(
                canvas_pos.x() + (obj_cam.get_window_origin().x()),
                canvas_pos.y() + (cam.get_window_height() - obj_cam.get_window_origin().y() -
                                  obj_cam.get_window_height()),
                float(obj_cam.get_window_width()),
                float(obj_cam.get_window_height()));
        }
    }


    ImGuizmo::OPERATION op = ImGuizmo::BOUNDS;

    switch (mode) {
    case GizmoMode::TRANSLATE: op = ImGuizmo::TRANSLATE; break;
    case GizmoMode::ROTATE: op = ImGuizmo::ROTATE; break;
    case GizmoMode::SCALE: op = ImGuizmo::SCALE; break;
    default: break;
    }


    // Inactive state
    if (!ctx.active && !ImGuizmo::IsUsing()) {
        /*
            Selection centroid
        */
        Eigen::Vector3f avg_pos = Eigen::Vector3f::Zero();
        view.each([&](Entity e, const Selected& sel, Transform& t) {
            auto bounds = registry.try_get<Bounds>(e);

            if (bounds) {
                avg_pos += bounds->global.center();
            } else {
                avg_pos += t.global.matrix().col(3).hnormalized();
            }
        });
        avg_pos *= 1.0f / float(selected_count);

        ctx.transform_start = Eigen::Translation3f(avg_pos);
        ctx.current_transform = ctx.transform_start;

    }
    // Just activated
    else if (!ctx.active && ImGuizmo::IsUsing()) {
        ctx.active = true;

        // Save initial transforms
        for (auto e : view) {
            LA_ASSERT(!registry.has<GizmoObjectTransform>(e));

            GizmoObjectTransform got;
            got.initial_transform = view.get<Transform>(e);
            got.parent_inverse =
                (got.initial_transform.global * got.initial_transform.local.inverse()).inverse();

            registry.emplace<GizmoObjectTransform>(e, got);
        }
    }
    // Just deactivated
    else if (ctx.active && !ImGuizmo::IsUsing()) {
        ctx.active = false;
        registry.clear<GizmoObjectTransform>();
    }


    AABB bbox;
    view.each([&](Entity e, const Selected& sel, Transform& t) {
        auto bb = registry.try_get<Bounds>(e);
        if (bb) bbox.extend(bb->global);
    });


    Eigen::Vector3f bbox_center = bbox.center();
    Eigen::Matrix<float, 2, 3, Eigen::RowMajor> bb;
    bb << (bbox.min() - bbox_center).transpose(), (bbox.max() - bbox_center).transpose();

    ImGuizmo::Manipulate(
        V.data(),
        P.data(),
        op,
        ImGuizmo::WORLD,
        ctx.current_transform.data(),
        nullptr,
        nullptr,
        (op == ImGuizmo::SCALE) ? bb.data() : nullptr,
        nullptr);

    if (ctx.active) {
        // Set final transform without initial gizmo transform
        Eigen::Affine3f inv_start = ctx.transform_start.inverse();

        auto active_transform_view = registry.view<Transform, GizmoObjectTransform>();
        active_transform_view.each([&](Entity e, Transform& t, GizmoObjectTransform& got) {
            // Initial -> origin to gizmo center -> gizmo transform
            const Eigen::Affine3f M =
                ctx.current_transform * inv_start * got.initial_transform.global;

            const Eigen::Affine3f local_transform = got.parent_inverse * M;

            const bool transform_changed =
                !(local_transform.matrix().array() == t.local.matrix().array()).all();

            if (transform_changed) {
                Transform new_t = t;
                new_t.local = got.parent_inverse * M;
                registry.replace<Transform>(e, new_t);
            }
        });
    }
}

bool gizmo_system_is_using()
{
    return ImGuizmo::IsUsing();
}

bool gizmo_system_is_over()
{
    return ImGuizmo::IsOver();
}

void gizmo_system_set_draw_list()
{
    ImGuizmo::SetDrawlist();
}

} // namespace ui
} // namespace lagrange

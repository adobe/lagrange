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
#include <lagrange/ui/Entity.h>
#include <lagrange/ui/components/CameraComponents.h>
#include <lagrange/ui/components/Common.h>
#include <lagrange/ui/default_events.h>
#include <lagrange/ui/systems/camera_systems.h>
#include <lagrange/ui/types/Camera.h>
#include <lagrange/ui/utils/bounds.h>
#include <lagrange/ui/utils/events.h>


namespace lagrange {
namespace ui {


void camera_controller_system(Registry& registry)
{
    auto view = registry.view<CameraController, Camera>();

    for (auto e : view) {
        bool changed = false;

        auto& camera = view.get<Camera>(e);
        auto& controller = view.get<CameraController>(e);

        controller.any_control_active =
            controller.dolly_active || controller.pan_active || controller.rotation_active;

        if (controller.dolly_active) {
            auto delta = controller.dolly_delta;

            if (!controller.ortho_interaction_2D) {
                if (controller.fov_zoom) {
                    camera.zoom(delta);
                } else {
                    camera.dolly(delta);
                }
            } else {
                Eigen::Vector2f origin_window = Eigen::Vector2f(
                    controller.mouse_current.x() / float(camera.get_window_width()),
                    controller.mouse_current.y() / float(camera.get_window_height()));

                Eigen::Vector4f viewport_adjust = Eigen::Vector4f(
                    origin_window.x(),
                    origin_window.x(),
                    origin_window.y(),
                    origin_window.y());
                Eigen::Vector4f v = camera.get_ortho_viewport();
                v -= viewport_adjust;
                camera.set_ortho_viewport((v * (1.0f - delta / 5.0f)) + viewport_adjust);
            }

            changed = true;
        }

        if (controller.pan_active) {
            const auto distance_multiplier = (camera.get_position() - camera.get_lookat()).norm();
            Eigen::Vector2f dscreen =
                (controller.mouse_delta * controller.pan_speed * distance_multiplier);
            Eigen::Vector3f side = camera.get_direction().cross(camera.get_up()).normalized();
            Eigen::Vector3f dpos = camera.get_up() * dscreen.y() + side * -dscreen.x();
            camera.set_position(camera.get_position() + dpos);
            camera.set_lookat(camera.get_lookat() + dpos);
            changed = true;
        }

        if (controller.rotation_active) {
            Eigen::Vector2f p = controller.mouse_current;
            Eigen::Vector2f delta = controller.mouse_delta;

            Eigen::Vector2f d = Eigen::Vector2f(
                delta.x() / camera.get_window_width(),
                delta.y() / camera.get_window_height());

            if (camera.get_type() == Camera::Type::PERSPECTIVE ||
                !controller.ortho_interaction_2D) {
                Eigen::Vector2f angle = (d * 4.0f * float(camera.get_retina_scale()));

                std::swap(angle.x(), angle.y());
                angle.y() = -angle.y();

                switch (camera.get_rotation_mode()) {
                case Camera::RotationMode::TUMBLE:
                    camera.rotate_tumble(angle.y(), angle.x());
                    break;
                case Camera::RotationMode::TURNTABLE:
                    camera.rotate_turntable(angle.y(), angle.x());
                    break;
                case Camera::RotationMode::ARCBALL:
                    camera.rotate_arcball(
                        controller.rotation_camera_pos_start,
                        controller.rotation_camera_up_start,
                        Eigen::Vector2f(
                            controller.rotation_mouse_start.x(),
                            controller.rotation_mouse_start.y()),
                        Eigen::Vector2f(p.x(), p.y()));
                    break;
                }
            } else if (camera.get_type() == Camera::Type::ORTHOGRAPHIC) {
                auto v = camera.get_ortho_viewport();
                float dx = -std::abs(v.x() - v.y());
                float dy = -std::abs(v.w() - v.z());
                v += Eigen::Vector4f(d.x() * dx, d.x() * dx, d.y() * dy, d.y() * dy);
                camera.set_ortho_viewport(v);
            }
            changed = true;
        }

        if (changed) {
            ui::publish<CameraChangedEvent>(registry, e);
        }
    }
}


void camera_turntable_system(Registry& registry)
{
    const auto dt = float(registry.ctx_or_set<GlobalTime>().dt);
    registry.view<Camera, CameraTurntable>().each(
        [&](Entity e, Camera& camera, CameraTurntable& ct) {
            if (ct.enabled) {
                camera.rotate_turntable(dt * ct.speed, 0, ct.axis);
                ui::publish<CameraChangedEvent>(registry, e);
            }
        });
}


void camera_focusfit_system(Registry& registry)
{
    AABB scene_bb = get_scene_bounding_box(registry);
    if (scene_bb.isEmpty()) {
        //If there's nothing in the scene, focus/fit unit box
        scene_bb = AABB(-Eigen::Vector3f::Ones(), Eigen::Vector3f::Ones());
    }

    const auto dt = float(registry.ctx<GlobalTime>().dt);
    const float eps = 0.0001f;
    auto cameras = registry.view<CameraFocusAndFit, Camera>();

    for (auto cam_e : cameras) {
        // Stop focus/fit if the camera is being used in viewport
        if (registry.all_of<CameraController>(cam_e) &&
            registry.get<CameraController>(cam_e).any_control_active) {
            registry.remove<CameraFocusAndFit>(cam_e);
            continue;
        }


        AABB focus_bb = scene_bb;
        bool changed = false;

        auto& ff = registry.get<CameraFocusAndFit>(cam_e);

        // Recompute bounds if there's a filter set
        if (ff.filter) {
            Eigen::AlignedBox3f filtered_bb;
            registry.view<const Bounds>().each([&](Entity e, const Bounds& bounds) {
                if (!ff.filter(registry, e) || bounds.bvh_node.isEmpty()) return;
                filtered_bb.extend(bounds.bvh_node);
            });
            if (!filtered_bb.isEmpty()) focus_bb = AABB(filtered_bb);
        }

        auto& cam = registry.get<Camera>(cam_e);
        const float radius = std::max(focus_bb.diagonal().norm() / 2.0f, 0.0001f);

        const float t = ff.current_time;
        bool fit_reached = false;
        bool focus_reached = false;

        if (ff.focus) {
            const auto old_lookat = cam.get_lookat();
            const auto new_lookat = focus_bb.center();
            focus_reached = (old_lookat - new_lookat).squaredNorm() < eps;
            cam.set_lookat(t * new_lookat + (1.0f - t) * old_lookat);
            changed = true;
        }


        if (ff.fit) {
            if (cam.get_type() == Camera::Type::PERSPECTIVE) {
                if (registry.all_of<CameraController>(cam_e) &&
                    registry.get<CameraController>(cam_e).fov_zoom) {
                    const float dist = (focus_bb.center() - cam.get_position()).norm();
                    if (dist > eps) {
                        const float fov = 2.0f * std::atan(2.0f * radius / dist);
                        const float old_fov = cam.get_fov();
                        fit_reached = std::abs(old_fov - fov) < eps;
                        cam.set_fov(t * fov + (1.0f - t) * old_fov);
                    }
                } else {
                    const float dist = (radius * 2.0f) / std::tan(cam.get_fov() / 2.0f);
                    const auto dir = cam.get_direction().normalized().eval();
                    const Eigen::Vector3f new_pos = cam.get_lookat() - dir * dist;
                    const Eigen::Vector3f old_pos = cam.get_position();

                    fit_reached = (old_pos - new_pos).squaredNorm() < eps;
                    cam.set_position(t * new_pos + (1.0f - t) * cam.get_position());
                }
            } else {
                Eigen::Vector4f v = cam.get_ortho_viewport();
                v *= radius / v.minCoeff();
                cam.set_ortho_viewport(v);
                fit_reached = true;
            }
            changed = true;
        }

        // Advance time
        if (ff.speed == std::numeric_limits<float>::infinity()) {
            ff.current_time = 1.00f;
        } else {
            ff.current_time += ff.speed * dt;
        }
        ff.current_time = std::min(1.0f, ff.current_time);

        bool fit_focus_ended =
            ((ff.fit && fit_reached) || !ff.fit) && ((ff.focus && focus_reached) || !ff.focus);

        if (fit_focus_ended || ff.current_time >= 1.0f) {
            registry.remove<CameraFocusAndFit>(cam_e);
        }

        if (changed) {
            ui::publish<CameraChangedEvent>(registry, cam_e);
        }
    }
}

} // namespace ui
} // namespace lagrange

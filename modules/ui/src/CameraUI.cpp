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
#include <lagrange/ui/CameraUI.h>
#include <lagrange/ui/GLContext.h>
#include <lagrange/ui/Camera.h>
#include <lagrange/utils/utils.h>

#include <IconsFontAwesome5.h>
#include <imgui.h>



namespace lagrange {
namespace ui {

void CameraUI::draw()
{
    auto& cam = get();

    this->begin();


    ImGui::Text("%s", "Rotation Type");
    auto rotation_mode = cam.get_rotation_mode();
    if (ImGui::RadioButton(
            "Tumble (Blender/Maya-like)", rotation_mode == Camera::RotationMode::TUMBLE)) {
        cam.set_rotation_mode(Camera::RotationMode::TUMBLE);
    }

    if (ImGui::RadioButton("Turntable", rotation_mode == Camera::RotationMode::TURNTABLE)) {
        cam.set_rotation_mode(Camera::RotationMode::TURNTABLE);
    }

    if (ImGui::RadioButton(
            "Arcball (Meshlab-like)", rotation_mode == Camera::RotationMode::ARCBALL)) {
        cam.set_rotation_mode(Camera::RotationMode::ARCBALL);
    }

    ImGui::Separator();

    Eigen::Vector3f pos = cam.get_position();
    Eigen::Vector3f lookat = cam.get_lookat();
    Eigen::Vector3f up = cam.get_up();

    Eigen::Vector3f side = cam.get_up().cross(cam.get_direction()).normalized();

    if (ImGui::DragFloat3("Pos", pos.data(), 0.01f)) {
        cam.set_position(pos);
    }
    if (ImGui::DragFloat3("Lookat", lookat.data(), 0.01f)) {
        cam.set_lookat(lookat);
    }
    if (ImGui::DragFloat3("Up", up.data(), 0.01f)) {
        cam.set_up(up);
    }
    ImGui::DragFloat3("Side/Right", side.data(), 0.01f);


    Eigen::Vector3f look_vec_n = (cam.get_position() - cam.get_lookat()).normalized();
    float yaw = lagrange::to_degrees(std::atan2(look_vec_n.z(), look_vec_n.x())); // azimuth
    float pitch = lagrange::to_degrees(std::acos(look_vec_n.y())); // inclination

    ImGui::DragFloat("Yaw/Azimuth", &yaw, 0.01f, 0, 0, "%.3f degrees");
    ImGui::DragFloat("Pitch/Inclination", &pitch, 0.01f, 0, 0, "%.3f degrees");


    ImGui::Separator();
    ImGui::Text(
        "Type: %s", cam.get_type() == Camera::Type::PERSPECTIVE ? "Perspective" : "Orthographic");
    auto ortho_viewport = cam.get_ortho_viewport();
    if (ImGui::DragFloat4("Orthographic viewport", ortho_viewport.data(), 0.01f)) {
        cam.set_ortho_viewport(ortho_viewport);
    }
    ImGui::Separator();
    ImGui::Text("Control:");
    ImGui::DragFloat("Rotation Speed", &m_control.rotate_speed, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("Zoom Speed", &m_control.zoom_speed, 0.01f, 0.0f, 1.0f);

    ImGui::Separator();
    ImGui::Text("Turntable:");
    if (ImGui::Checkbox("Enabled", &m_turntable.enabled)) {
        if (m_turntable.enabled) m_turntable.start_pos = cam.get_position();
    }
    ImGui::DragFloat("Time", &m_turntable.t, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Speed", &m_turntable.speed, 0.01f, 0.0f, 10.0f);

    if (ImGui::DragFloat3("Axis", m_turntable.axis.data(), 0.01f))
        m_turntable.axis = m_turntable.axis.normalized();
    ImGui::Separator();
    ImGui::Text("Projection matrix");
    float fov = lagrange::to_degrees(cam.get_fov());

    if (ImGui::DragFloat("fov degrees", &fov, 0.1f)) {
        cam.set_fov(lagrange::to_radians(fov));
    }
    float near_plane = cam.get_near();
    float far_plane = cam.get_far();
    if (ImGui::DragFloat("near plane", &near_plane) || ImGui::DragFloat("far plane", &far_plane)) {
        cam.set_planes(near_plane, far_plane);
    }

    float width = cam.get_window_width();
    float height = cam.get_window_height();
    if (ImGui::DragFloat("width", &width) || ImGui::DragFloat("height", &height)) {
        cam.set_window_dimensions(std::floor(width), std::floor(height));
    }


    this->end();
}

void CameraUI::update(double dt)
{
    if (m_turntable.enabled) {
        auto& cam = get();
        cam.rotate_turntable(float(dt) * m_turntable.speed, 0, m_turntable.axis);
    }
}

} // namespace ui
} // namespace lagrange

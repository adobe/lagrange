/*
 * Copyright 2019 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/ui/imgui/UIWidget.h>

#include <lagrange/ui/types/Color.h>
#include <lagrange/ui/types/GLContext.h>
#include <lagrange/ui/types/Keybinds.h>
#include <lagrange/ui/types/Material.h>
#include <lagrange/ui/types/Texture.h>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace lagrange {
namespace ui {

bool UIWidget::operator()(std::string& value)
{
    return ImGui::InputTextMultiline(m_name.c_str(), &value);
}

bool UIWidget::operator()(double& value)
{
    float fv = static_cast<float>(value);
    bool changed = (*this)(fv);
    if (changed) {
        value = static_cast<double>(fv);
        return true;
    }
    return false;
}

bool UIWidget::operator()(bool& value)
{
    bool res = ImGui::Checkbox(m_name.c_str(), &value);
    return res;
}

bool UIWidget::operator()(int& value)
{
    return ImGui::InputInt(m_name.c_str(), &value, value / 100);
}

bool UIWidget::operator()(float& value)
{
    return ImGui::InputFloat(
        m_name.c_str(),
        &value,
        value / 100.0f,
        0.0f,
        value > 0.001f ? "%.3f" : "%.7f");
}

bool UIWidget::operator()(Color& value)
{
    const auto text_size = ImGui::CalcTextSize(m_name.c_str());
    ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x * 0.7f, text_size.y);

    ImGui::PushID(&value);

    if (ImGui::ColorButton(
            m_name.c_str(),
            ImColor(value.r(), value.g(), value.b(), value.a()),
            0,
            size)) {
        ImGui::OpenPopup(m_name.c_str());
    }

    bool changed = false;

    if (ImGui::BeginPopupContextItem(m_name.c_str())) {
        // If not default invisible label
        if (m_name != "##value") {
            ImGui::Text("%s", m_name.c_str());
        }
        changed = ImGui::ColorPicker4("##solidcolor", value.data());
        ImGui::EndPopup();
    }

    if (m_name != "##value") {
        ImGui::SameLine();
        ImGui::Text("%s", m_name.c_str());
    }

    ImGui::PopID();

    return changed;
}

bool UIWidget::operator()(Texture& value, int width /* = 0*/, int height /* = 0*/)
{
    if (value.get_params().type != GL_TEXTURE_2D &&
        value.get_params().type != GL_TEXTURE_CUBE_MAP) {
        ImGui::Text("Texture type not supported");
        return false;
    }

    if (value.get_params().type == GL_TEXTURE_CUBE_MAP) {
        ImGui::PushID(value.get_id());
        bool res =
            ImGui::Button("Cubemap", ImVec2(static_cast<float>(width), static_cast<float>(height)));
        ImGui::PopID();
        return res;
    }

    ImTextureID texID = reinterpret_cast<void*>((long long int)(value.get_id()));
    ImVec2 uv0 = ImVec2(0, 1);
    ImVec2 uv1 = ImVec2(1, 0);

    ImGui::Image(
        texID,
        ImVec2(float(width), float(height)),
        uv0,
        uv1,
        ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
        ImVec4(0.0f, 0.0f, 0.0f, 0.5f));

    return ImGui::IsItemClicked(0) || ImGui::IsItemClicked(1);
    /*if (ImGui::BeginPopupContextItem(("Texture " + m_name).c_str())) {
        ImGui::Text("%s", m_name.c_str());
        ImGui::Image(texID, ImVec2(800, 800), uv0, uv1, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(0.0f,
    0.0f, 0.0f, 0.5f)); ImGui::EndPopup();
    }*/

    return false;
}

bool UIWidget::operator()(Eigen::Vector2f& value)
{
    return ImGui::DragFloat2(m_name.c_str(), value.data(), 0.1f);
}

bool UIWidget::operator()(Eigen::Vector3f& value)
{
    return ImGui::DragFloat3(m_name.c_str(), value.data(), 0.1f);
}

bool UIWidget::operator()(Eigen::Vector4f& value)
{
    return ImGui::DragFloat4(m_name.c_str(), value.data(), 0.1f);
}

bool UIWidget::operator()(Eigen::Vector2i& value)
{
    return ImGui::DragInt2(m_name.c_str(), value.data(), 1);
}

bool UIWidget::operator()(Eigen::Vector3i& value)
{
    return ImGui::DragInt3(m_name.c_str(), value.data(), 1);
}

bool UIWidget::operator()(Eigen::Vector4i& value)
{
    return ImGui::DragInt4(m_name.c_str(), value.data(), 1);
}

bool UIWidget::operator()(Eigen::Matrix2f& value)
{
    return render_matrix(value.data(), 2);
}

bool UIWidget::operator()(Eigen::Matrix3f& value)
{
    return render_matrix(value.data(), 3);
}

bool UIWidget::operator()(Eigen::Matrix4f& value)
{
    return render_matrix(value.data(), 4);
}

bool UIWidget::operator()(Eigen::Affine3f& value)
{
    return (*this)(value.matrix());
}

bool UIWidget::render_matrix(float* M, int dimension)
{
    int ID = static_cast<int>(reinterpret_cast<size_t>(M));
    ImGui::BeginChildFrame(
        ID,
        ImVec2(
            ImGui::GetColumnWidth() - 17,
            ImGui::GetTextLineHeightWithSpacing() * dimension + 5));
    ImGui::Columns(dimension);

    bool changed = false;
    for (int k = 0; k < dimension; k++) {
        for (int i = 0; i < dimension; i++) {
            ImGui::PushID(k * dimension + i);
            changed |= ImGui::DragFloat(m_name.c_str(), M + k + i * dimension);
            ImGui::PopID();
        }
        if (k < dimension - 1) ImGui::NextColumn();
    }

    ImGui::Columns(1);
    ImGui::EndChildFrame();
    return changed;
}

UIWidget::UIWidget(const std::string& name /*= "##value"*/)
    : m_name(name)
{}

} // namespace ui
} // namespace lagrange

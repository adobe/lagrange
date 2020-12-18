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
#pragma once

#include <imgui.h>
#include <lagrange/ui/ui_common.h>
#include <lagrange/ui/utils/math.h>
#include <lagrange/utils/strings.h>

#include <memory>
#include <string>
#include <unordered_set>

namespace lagrange {
namespace ui {

class Material;
class Texture;
class Color;
class OptionSet;
class Keybinds;

class UIWidget
{
public:
    /// Constructs a widget with name, if no name given an invisible
    /// label is used
    UIWidget(const std::string& name = "##value");

    template <typename T>
    bool operator()(T& LGUI_UNUSED(value))
    {
        return false;
    }

    bool operator()(float& value);
    bool operator()(int& value);
    bool operator()(bool& value);
    bool operator()(double& value);
    bool operator()(std::string& value);

    bool operator()(Material& value, int dim = 3, bool unused = false);
    bool operator()(Texture& value, int width = 0, int height = 0);
    bool operator()(Color& value);
    bool operator()(
        OptionSet& value, const std::string& name = "", int depth = 0, bool selectable = true);

    bool operator()(Eigen::Vector2f& value);
    bool operator()(Eigen::Vector3f& value);
    bool operator()(Eigen::Vector4f& value);
    bool operator()(Eigen::Vector2i& value);
    bool operator()(Eigen::Vector3i& value);
    bool operator()(Eigen::Vector4i& value);
    bool operator()(Eigen::Matrix2f& value);
    bool operator()(Eigen::Matrix3f& value);
    bool operator()(Eigen::Matrix4f& value);
    bool operator()(Eigen::Affine3f& value);

    static bool button_toolbar(bool selected,
        const std::string& label,
        const std::string& tooltip = "",
        const std::string& keybind_id = "",
        const Keybinds* keybinds = nullptr,
        bool enabled = true);

    static bool button_icon(bool selected,
        const std::string& label,
        const std::string& tooltip = "",
        const std::string& keybind_id = "",
        const Keybinds* keybinds = nullptr,
        bool enabled = true,
        ImVec2 size = ImVec2(0, 0));

private:
    bool render_matrix(float* M, int dimension);

    const std::string& m_name;
};


namespace utils {
template <typename T>
struct value_field
{
    bool draw(T& val);
};

template <>
struct value_field<float>
{
    bool draw(float& val) { return ImGui::DragFloat("##", &val, val / 100.0f); }
};

template <>
struct value_field<int>
{
    bool draw(int& val) { return ImGui::DragInt("##", &val); }
};

template <>
struct value_field<double>
{
    bool draw(double& val)
    {
        float f = static_cast<float>(val);
        if (ImGui::DragFloat("##", &f, f / 100.0f)) {
            val = static_cast<double>(f);
            return true;
        }
        return false;
    }
};
} // namespace utils

class PaginatedMatrixWidget
{
public:
    template <typename MatrixType>
    bool operator()(const MatrixType& matrix,
        int& row_out,
        int& col_out,
        typename MatrixType::Scalar& value_out)
    {
        const int pages = (int(matrix.rows()) + m_per_page - 1) / m_per_page;
        ImGui::DragInt(
            string_format("/ {} pages", pages).c_str(), &m_current_page, 1, 0, pages - 1);

        m_current_page = std::max(int(0), std::min(m_current_page, pages - 1));

        ImGui::Columns(int(matrix.cols()) + 1);

        int begin = m_current_page * m_per_page;
        int end = std::min(int((m_current_page + 1) * m_per_page), static_cast<int>(matrix.rows()));

        bool changed = false;

        for (auto i = begin; i < end; i++) {
            ImGui::Text("[%d]", i);
            ImGui::NextColumn();

            for (auto k = 0; k < matrix.cols(); k++) {
                typename MatrixType::Scalar new_value;
                if (matrix_field(matrix, i, k, new_value)) {
                    row_out = i;
                    col_out = k;
                    value_out = new_value;
                    changed = true;
                }
                ImGui::NextColumn();
            }
        }
        ImGui::Columns(1);
        return changed;
    }


    template <typename MatrixType, typename IndexType>
    bool operator()(const MatrixType& matrix,
        const std::unordered_set<IndexType>& selection,
        int& row_out,
        int& col_out,
        typename MatrixType::Scalar& value_out)
    {
        const int pages = (int(selection.size()) + m_per_page - 1) / m_per_page;
        ImGui::DragInt(string_format("/ {} pages (Selection)", pages).c_str(),
            &m_current_page,
            1,
            0,
            pages - 1);

        m_current_page = std::max(int(0), std::min(m_current_page, pages - 1));

        ImGui::Columns(int(matrix.cols()) + 1);

        int begin = m_current_page * m_per_page;
        int end = std::min(int((m_current_page + 1) * m_per_page), static_cast<int>(matrix.rows()));

        bool changed = false;

        int cnt = -1;
        for (auto elem : selection) {
            cnt++;
            if (cnt < begin) continue;
            if (cnt == end) break;

            ImGui::Text("[%d]", int(elem));
            ImGui::NextColumn();

            for (auto k = 0; k < matrix.cols(); k++) {
                typename MatrixType::Scalar new_value;
                if (matrix_field(matrix, elem, k, new_value)) {
                    row_out = elem;
                    col_out = k;
                    value_out = new_value;
                    changed = true;
                }
                ImGui::NextColumn();
            }
        }
        ImGui::Columns(1);
        return changed;
    }


    int get_per_page() const { return m_per_page; }
    void set_per_page(int value) { m_per_page = value; }

private:
    template <typename MatrixType>
    bool matrix_field(const MatrixType& matrix, int row, int col, typename MatrixType::Scalar& out)
    {
        ImGui::PushID(row * int(matrix.cols()) + col);
        auto val = matrix(row, col);
        bool changed = utils::value_field<typename MatrixType::Scalar>().draw(val);
        if (changed) {
            out = val;
        }
        ImGui::PopID();
        return changed;
    }

    int m_current_page = 0;
    int m_per_page = 25;
};


} // namespace ui
} // namespace lagrange

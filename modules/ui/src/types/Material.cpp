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

#include <lagrange/ui/types/Material.h>

namespace lagrange {
namespace ui {

Material::Material(Registry& r, StringID shader_id)
    : m_shader_id(shader_id)
{
    auto shader = ui::get_shader(r, shader_id);
    if (!shader) {
        return;
    }

    // Populate defaults from shader
    for (const auto& it : shader->float_properties()) {
        float_values[it.first] = it.second.default_value;
    }
    for (const auto& it : shader->texture_properties()) {
        texture_values[it.first] = it.second.default_value;
    }
    for (const auto& it : shader->color_properties()) {
        color_values[it.first] = it.second.default_value;
    }
    for (const auto& it : shader->vector_properties()) {
        vec4_values[it.first] = it.second.default_value;
    }
}

StringID Material::shader_id() const
{
    return m_shader_id;
}

void Material::set_vec4(StringID id, const Eigen::Vector4f& vec)
{
    vec4_values[id] = vec;
}

void Material::set_vec4(const std::string& name, const Eigen::Vector4f& vec)
{
    set_vec4(string_id(name), vec);
}

void Material::set_vec4_array(StringID id, const Eigen::Vector4f* vectors, size_t N)
{
    auto& arr = vec4_array_values[id];
    arr.resize(N);
    for (size_t i = 0; i < N; i++) arr[i] = vectors[i];
}

void Material::set_vec4_array(const std::string& name, const Eigen::Vector4f* vectors, size_t N)
{
    set_vec4_array(string_id(name), vectors, N);
}

void Material::set_mat4(StringID id, const Eigen::Matrix4f& matrix)
{
    mat4_values[id] = matrix;
}

void Material::set_mat4(const std::string& name, const Eigen::Matrix4f& matrix)
{
    set_mat4(string_id(name), matrix);
}

void Material::set_mat4_array(StringID id, const Eigen::Matrix4f* matrices, size_t N)
{
    auto& arr = mat4_array_values[id];
    arr.clear();
    for (size_t i = 0; i < N; i++) arr.push_back(matrices[i]);
}

void Material::set_mat4_array(const std::string& name, const Eigen::Matrix4f* matrices, size_t N)
{
    set_mat4_array(string_id(name), matrices, N);
}

void Material::set_color(StringID id, Color color)
{
    // Find color property
    auto it = color_values.find(id);
    if (it == color_values.end()) {
        // If not found, try to find texture property and set its fallback color
        auto it_tex = texture_values.find(id);
        if (it_tex == texture_values.end()) return;
        it_tex->second.color = std::move(color);
        return;
    }
    it->second = std::move(color);
}

void Material::set_color(const std::string& name, Color color)
{
    return set_color(string_id(name), color);
}

void Material::set_texture(StringID id, std::shared_ptr<Texture> texture)
{
    auto it_tex = texture_values.find(id);
    if (it_tex == texture_values.end()) return;
    it_tex->second.texture = std::move(texture);
}

void Material::set_texture(const std::string& name, std::shared_ptr<Texture> texture)
{
    return set_texture(string_id(name), texture);
}

void Material::set_float(StringID id, float value)
{
    // Find float property
    auto it = float_values.find(id);
    if (it == float_values.end()) {
        // If not found, try to find texture property and set its fallback red value
        auto it_tex = texture_values.find(id);
        if (it_tex == texture_values.end()) {
            //Set as is, for user uniforms
            float_values[id] = value;
            return;
        }
        it_tex->second.color.x() = value;
        return;
    }
    it->second = value;
}

void Material::set_float(const std::string& name, float value)
{
    set_float(string_id(name), value);
}

void Material::set_int(StringID id, int value)
{
    int_values[id] = value;
}

void Material::set_int(const std::string& name, int value)
{
    set_int(string_id(name), value);
}

} // namespace ui
} // namespace lagrange

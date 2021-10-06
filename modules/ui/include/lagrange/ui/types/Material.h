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

#include <lagrange/ui/Entity.h>
#include <lagrange/ui/types/ShaderLoader.h>

namespace lagrange {
namespace ui {


class Material
{
public:
    Material(Registry& r, StringID shader_id, const ShaderDefines & shader_defines = {});

    StringID shader_id() const;

    std::unordered_map<StringID, int> int_values;
    std::unordered_map<StringID, float> float_values;
    std::unordered_map<StringID, Color> color_values;
    std::unordered_map<StringID, ShaderTextureValue> texture_values;
    std::unordered_map<StringID, Eigen::Matrix4f> mat4_values;
    std::unordered_map<StringID, std::vector<Eigen::Matrix4f>> mat4_array_values;
    std::unordered_map<StringID, Eigen::Vector4f> vec4_values;
    std::unordered_map<StringID, std::vector<Eigen::Vector4f>> vec4_array_values;

    void set_vec4(const std::string& name, const Eigen::Vector4f& vec);
    void set_vec4(StringID id, const Eigen::Vector4f& vec);
    void set_vec4_array(const std::string& name, const Eigen::Vector4f* vectors, size_t N);
    void set_vec4_array(StringID id, const Eigen::Vector4f* vectors, size_t N);

    void set_mat4(const std::string& name, const Eigen::Matrix4f& matrix);
    void set_mat4(StringID id, const Eigen::Matrix4f& matrix);
    void set_mat4_array(const std::string& name, const Eigen::Matrix4f* matrices, size_t N);
    void set_mat4_array(StringID id, const Eigen::Matrix4f* matrices, size_t N);

    void set_color(const std::string& name, Color color);
    void set_color(StringID id, Color color);
    void set_texture(const std::string& name, std::shared_ptr<Texture> texture);
    void set_texture(StringID id, std::shared_ptr<Texture> texture);

    void set_float(const std::string& name, float value);
    void set_float(StringID id, float value);

    void set_int(const std::string& name, int value);
    void set_int(StringID id, int value);
    
    

private:
    StringID m_shader_id;
};

} // namespace ui
} // namespace lagrange

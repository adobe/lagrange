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

#include <GL/gl3w.h>

#include <lagrange/ui/utils/math.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace lagrange {
namespace ui {

enum ShaderInterface {
    SHADER_INTERFACE_NONE = 0,
    SHADER_INTERFACE_UNIFORM = 1,
    SHADER_INTERFACE_ATTRIB = 2
};

struct ShaderValue
{
    int location;
    int size;
    GLenum type;
    ShaderInterface shaderInterface;
    ShaderValue& operator=(Eigen::Vector2f val);
    ShaderValue& operator=(Eigen::Vector3f val);
    ShaderValue& operator=(Eigen::Vector4f val);

    ShaderValue& operator=(Eigen::Matrix2f val);
    ShaderValue& operator=(Eigen::Matrix3f val);
    ShaderValue& operator=(Eigen::Matrix4f val);
    ShaderValue& operator=(Eigen::Affine3f val);
    ShaderValue& operator=(Eigen::Projective3f val);


    ShaderValue& operator=(double val);
    ShaderValue& operator=(float val);
    ShaderValue& operator=(int val);
    ShaderValue& operator=(bool val);


    ShaderValue& operator=(const std::vector<Eigen::Vector3f>& arr);


    ShaderValue& set_array(const float* data, int n);
    ShaderValue& set_array(const int* data, int n);
    ShaderValue& set_array(const unsigned int* data, int n);

    // NO templated Eigen matrix base, reason:
    // If passing things to shader,
    // pass it using the exact type to avoid mistakes

    ShaderValue& set_vectors(const Eigen::Vector2f* data, int n);
    ShaderValue& set_vectors(const Eigen::Vector3f* data, int n);
    ShaderValue& set_vectors(const Eigen::Vector4f* data, int n);
    ShaderValue& set_matrices(const Eigen::Matrix2f* data, int n, bool transpose = false);
    ShaderValue& set_matrices(const Eigen::Matrix3f* data, int n, bool transpose = false);
    ShaderValue& set_matrices(const Eigen::Matrix4f* data, int n, bool transpose = false);
    ShaderValue& set_matrices(const Eigen::Affine3f* data, int n, bool transpose = false);


    static ShaderValue none;
};

class ShaderException : public std::runtime_error
{
public:
    ShaderException(const char* str)
        : std::runtime_error(str)
    {}

    void set_desc(const std::string& desc) { m_desc = desc; }
    const std::string& get_desc() { return m_desc; }

private:
    std::string m_desc;
};

using ShaderDefines = std::vector<std::pair<std::string, std::string>>;

class Shader
{
public:
    Shader(const std::string& code, const ShaderDefines& defines); // throws

    bool bind() const;
    static void unbind();

    ShaderValue& operator[](const std::string& name);

    const std::string& get_source() const;
    std::string& get_source();

    const ShaderDefines& get_defines() const;

private:
    GLuint m_id;
    std::unordered_map<std::string, ShaderValue> m_resources;
    std::string m_source;
    ShaderDefines m_defines;
};

} // namespace ui
} // namespace lagrange

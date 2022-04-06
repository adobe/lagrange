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

#if defined(__EMSCRIPTEN__)
#include <webgl/webgl2.h>
#else
#include <GL/gl3w.h>
#endif

#include <lagrange/ui/Entity.h>
#include <lagrange/ui/utils/math.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <lagrange/ui/types/Color.h>
#include <lagrange/ui/types/Texture.h>
#include <typeindex>
#include <variant>

#include <lagrange/Logger.h>

namespace lagrange {
namespace ui {


struct ShaderTextureValue
{
    std::shared_ptr<Texture> texture = nullptr;
    Texture::Transform transform;
    Eigen::Vector4f color;
};


template <typename T>
struct ShaderProperty
{
    using value_type = T;
    T default_value;
    std::string display_name;
};

struct ShaderColorProperty : public ShaderProperty<Color>
{
    bool is_attrib = false; // Use as vertex attribute default value, not uniform
};

struct ShaderVectorProperty : public ShaderProperty<Eigen::Vector4f>
{
};


struct ShaderTextureProperty : public ShaderProperty<ShaderTextureValue>
{
    int value_dimension = 0;
    GLenum sampler_type;

    /// Semantics:
    bool normal = false; // Is it a normal texture
    bool transformable = false; // Can 2D transform be used
    bool colormap = false; // Treat as colormap / transfer function
};

struct ShaderFloatProperty : public ShaderProperty<float>
{
    float min_value = std::numeric_limits<float>::lowest();
    float max_value = std::numeric_limits<float>::max();
};

struct ShaderBoolProperty : public ShaderProperty<bool>
{
};

struct ShaderIntProperty : public ShaderProperty<int>
{
    int min_value = std::numeric_limits<int>::lowest();
    int max_value = std::numeric_limits<int>::max();
};

//
struct GLQuery
{
    GLenum type;
    GLuint id;
    GLint result;
};

/// Rasterization pipeline option names
struct RasterizerOptions
{
    constexpr static const StringID DepthTest = "DepthTest"_hs;
    constexpr static const StringID DepthMask = "DepthMask"_hs;
    constexpr static const StringID DepthFunc = "DepthFunc"_hs;
    constexpr static const StringID BlendEquation = "BlendEquation"_hs;
    constexpr static const StringID DrawBuffer = "DrawBuffer"_hs;
    constexpr static const StringID ReadBuffer = "ReadBuffer"_hs;
    constexpr static const StringID CullFaceEnabled = "CullFaceEnabled"_hs;
    constexpr static const StringID CullFace = "CullFace"_hs;
    constexpr static const StringID ColorMask = "ColorMask"_hs;

    constexpr static const StringID ScissorTest = "ScissorTest"_hs;
    constexpr static const StringID ScissorX = "ScissorX"_hs;
    constexpr static const StringID ScissorY = "ScissorY"_hs;
    constexpr static const StringID ScissorWidth = "ScissorWidth"_hs;
    constexpr static const StringID ScissortHeight = "ScissortHeight"_hs;


    /// Blend func separate
    constexpr static const StringID BlendSrcRGB = "BlendSrcRGB"_hs;
    constexpr static const StringID BlendDstRGB = "BlendDstRGB"_hs;
    constexpr static const StringID BlendSrcAlpha = "BlendSrcAlpha"_hs;
    constexpr static const StringID BlendDstAlpha = "BlendDstAlpha"_hs;

    /// Will only render entities with GLQuery of the same type
    constexpr static const StringID Query = "Query"_hs;

    /// Use for transparency and polygon offset
    constexpr static const StringID Pass = "_Pass"_hs;

    /// Rasterize as point/line/polygon, values: GL_POINT/GL_LINE/GL_FILL. Applies to GL_FRONT_AND_BACK by default
    constexpr static const StringID PolygonMode = "_PolygonMode"_hs;

    constexpr static const StringID PointSize = "_PointSize"_hs;

    /// Not set by default, use to override
    constexpr static const StringID Primitive = "Primitive"_hs;
};

///

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
    const ShaderValue& operator=(Eigen::Vector2f val) const;
    const ShaderValue& operator=(Eigen::Vector3f val) const;
    const ShaderValue& operator=(Eigen::Vector4f val) const;

    const ShaderValue& operator=(Eigen::Matrix2f val) const;
    const ShaderValue& operator=(Eigen::Matrix3f val) const;
    const ShaderValue& operator=(Eigen::Matrix4f val) const;
    const ShaderValue& operator=(Eigen::Affine3f val) const;
    const ShaderValue& operator=(Eigen::Projective3f val) const;


    const ShaderValue& operator=(double val) const;
    const ShaderValue& operator=(float val) const;
    const ShaderValue& operator=(int val) const;
    const ShaderValue& operator=(bool val) const;


    const ShaderValue& operator=(const std::vector<Eigen::Vector3f>& arr) const;


    const ShaderValue& set_array(const float* data, int n) const;
    const ShaderValue& set_array(const int* data, int n) const;
    const ShaderValue& set_array(const unsigned int* data, int n) const;

    // NO templated Eigen matrix base, reason:
    // If passing things to shader,
    // pass it using the exact type to avoid mistakes

    const ShaderValue& set_vectors(const Eigen::Vector2f* data, int n) const;
    const ShaderValue& set_vectors(const Eigen::Vector3f* data, int n) const;
    const ShaderValue& set_vectors(const Eigen::Vector4f* data, int n) const;
    const ShaderValue& set_matrices(const Eigen::Matrix2f* data, int n, bool transpose = false)
        const;
    const ShaderValue& set_matrices(const Eigen::Matrix3f* data, int n, bool transpose = false)
        const;
    const ShaderValue& set_matrices(const Eigen::Matrix4f* data, int n, bool transpose = false)
        const;
    const ShaderValue& set_matrices(const Eigen::Affine3f* data, int n, bool transpose = false)
        const;


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

    const ShaderValue& operator[](const std::string& name);

    const std::string& get_source() const;
    std::string& get_source();

    const ShaderDefines& get_defines() const;

    const std::unordered_map<StringID, ShaderValue>& uniforms() const { return m_uniforms; }
    const std::unordered_map<StringID, ShaderValue>& attribs() const { return m_attribs; }
    const std::unordered_map<StringID, std::string>& names() const { return m_names; }

    const std::string& name(StringID id) const { return m_names.at(id); }

    const ShaderValue& uniform(const std::string& name) const { return uniform(string_id(name)); }

    const ShaderValue& uniform(StringID id) const
    {
        auto it = m_uniforms.find(id);
        if (it == m_uniforms.end()) return ShaderValue::none;
        return it->second;
    }

    const ShaderValue& attrib(const std::string& name) const { return attrib(string_id(name)); }

    const ShaderValue& attrib(StringID id) const
    {
        auto it = m_attribs.find(id);
        if (it == m_attribs.end()) return ShaderValue::none;
        return it->second;
    }

    const std::unordered_map<StringID, int>& sampler_indices() const { return m_sampler_indices; }

    const std::unordered_map<StringID, ShaderTextureProperty>& texture_properties() const
    {
        return m_texture_properties;
    }

    const std::unordered_map<StringID, ShaderFloatProperty>& float_properties() const
    {
        return m_float_properties;
    }

    const std::unordered_map<StringID, ShaderColorProperty>& color_properties() const
    {
        return m_color_properties;
    }

    const std::unordered_map<StringID, ShaderVectorProperty>& vector_properties() const
    {
        return m_vector_properties;
    }

    const std::unordered_map<StringID, ShaderBoolProperty>& bool_properties() const
    {
        return m_bool_properties;
    }

    const std::unordered_map<StringID, ShaderIntProperty>& int_properties() const
    {
        return m_int_properties;
    }

    void upload_default_values();

private:
    void process_properties(std::string& source);

    GLuint m_id;
    std::unordered_map<StringID, ShaderValue> m_uniforms;
    std::unordered_map<StringID, ShaderValue> m_attribs;
    std::unordered_map<StringID, int> m_sampler_indices;
    std::string m_source;
    ShaderDefines m_defines;

    std::unordered_map<StringID, ShaderTextureProperty> m_texture_properties;
    std::unordered_map<StringID, ShaderFloatProperty> m_float_properties;
    std::unordered_map<StringID, ShaderColorProperty> m_color_properties;
    std::unordered_map<StringID, ShaderVectorProperty> m_vector_properties;
    std::unordered_map<StringID, ShaderBoolProperty> m_bool_properties;
    std::unordered_map<StringID, ShaderIntProperty> m_int_properties;
    std::unordered_map<StringID, std::string> m_names;
};

} // namespace ui
} // namespace lagrange

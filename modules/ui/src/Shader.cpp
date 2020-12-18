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
#include <lagrange/ui/GLContext.h>
#include <lagrange/ui/Shader.h>

#include <cassert>
#include <iostream>
#include <regex>
#include <unordered_map>

namespace lagrange {
namespace ui {

const static std::unordered_map<GLenum, std::string> shaderEnumToString = {
    {GL_VERTEX_SHADER, "VERTEX"},
    {GL_FRAGMENT_SHADER, "FRAGMENT"},
    {GL_GEOMETRY_SHADER, "GEOMETRY"},
    {GL_TESS_CONTROL_SHADER, "TCS"},
    {GL_TESS_EVALUATION_SHADER, "TES"}};

bool is_whitespace_only(const std::string& s)
{
    if (s.find_first_not_of(' ') != std::string::npos) {
        return false;
    }
    if (s.find_first_not_of('\t') != std::string::npos) {
        return false;
    }
    if (s.find_first_not_of('\n') != std::string::npos) {
        return false;
    }
    if (s.find_first_not_of('\r') != std::string::npos) {
        return false;
    }
    return true;
}

std::unordered_map<GLenum, std::string> preprocessShaderCode(
    std::string code, const ShaderDefines& defines)
{
    const std::unordered_map<GLenum, std::regex> regexes = {
        {GL_VERTEX_SHADER, std::regex("#pragma +VERTEX")},
        {GL_FRAGMENT_SHADER, std::regex("#pragma +FRAGMENT")},
        {GL_GEOMETRY_SHADER, std::regex("#pragma +GEOMETRY")},
        {GL_TESS_CONTROL_SHADER, std::regex("#pragma +TCS")},
        {GL_TESS_EVALUATION_SHADER, std::regex("#pragma +TES")},
    };

    bool hasVertex = false;
    bool hasFragment = false;

    // Match pragmas delimiting individual shaders
    std::vector<std::pair<GLenum, std::smatch>> matches;
    for (auto it : regexes) {
        std::smatch match;
        if (std::regex_search(code, match, it.second)) {
            matches.push_back(std::make_pair(it.first, std::move(match)));
            if (it.first == GL_VERTEX_SHADER) hasVertex = true;
            if (it.first == GL_FRAGMENT_SHADER) hasFragment = true;
        }
    }

    // Invalid if less than two shaders found, or missing vertex and fragment
    if (matches.size() < 2 || !hasFragment || !hasVertex)
        return std::unordered_map<GLenum, std::string>(); // return empty

    // Sort by order of occurence
    std::sort(matches.begin(),
        matches.end(),
        [](std::pair<GLenum, std::smatch>& a, std::pair<GLenum, std::smatch>& b) {
            return a.second.position() < b.second.position();
        });


    // Extract common part
    std::string common = GLState::get_glsl_version_string() + "\n";

    for (auto & define : defines) {
        common += "#define " + define.first + " " + define.second + "\n";
    }
    common += code.substr(0, matches.front().second.position());


    // Extract individual shader sources
    std::unordered_map<GLenum, std::string> shaderSources;
    for (auto it = matches.begin(); it != matches.end(); it++) {
        auto next = it + 1;

        size_t begin = it->second.position() + it->second.length();

        auto sub_shader_source = code.substr(
            begin, (next == matches.end()) ? std::string::npos : (next->second.position() - begin));

        // Skip empty sub shader
        if (is_whitespace_only(sub_shader_source)) continue;

        shaderSources[it->first] = common + sub_shader_source;
    }

    return shaderSources;
}

std::string annotateLines(const std::string& str)
{
    std::string newstr = str;
    size_t index = 0;
    int line = 1;

    while (true) {
        index = newstr.find("\n", index);
        if (index == std::string::npos) break;
        char buf[16];
        sprintf(buf, "%d", line++);
        newstr.replace(index, 1, std::string("\n") + buf + std::string("\t"));

        index += strlen(buf) + 1 + 1;
    }

    return newstr;
}

Shader::Shader(const std::string& code, const ShaderDefines& defines)
    : m_source(code)
    , m_defines(defines)
{
    if (code.length() == 0) {
        throw ShaderException("Empty shader code");
    }

    auto shaderSources = preprocessShaderCode(code, m_defines);
    if (shaderSources.size() == 0) {
        throw ShaderException("Could not find vertex and fragment sub shaders.");
    }

    GLuint programID = glCreateProgram();
    std::unordered_map<GLenum, GLint> shaderIDs;

    // Cleanup in case of failure
    auto failFun = [&](const std::string& err) -> void {
        for (auto it : shaderIDs) glDeleteShader(it.second);
        shaderIDs.clear();

        glDeleteProgram(programID);
        programID = 0;

        throw ShaderException(err.c_str());
    };

    /*
        Compile individual shaders
    */
    std::string wholeShader = "";
    for (auto & it : shaderSources) {
        GLuint id = glCreateShader(it.first);

        wholeShader.append(it.second);
        // Send source
        {
            GLint sourceLength = static_cast<GLint>(it.second.length());
            const GLchar* sourcePtr = (const GLchar*)it.second.c_str();
            glShaderSource(id, 1, &sourcePtr, &sourceLength);
        }

        glCompileShader(id);

        GLint isCompiled = 0;
        glGetShaderiv(id, GL_COMPILE_STATUS, &isCompiled);

        if (isCompiled == GL_FALSE) {
            GLint maxLength = 0;
            glGetShaderiv(id, GL_INFO_LOG_LENGTH, &maxLength);

            std::vector<char> buf(maxLength);
            glGetShaderInfoLog(id, maxLength, &maxLength, reinterpret_cast<GLchar*>(buf.data()));

            failFun("Compile error:\n" + annotateLines(it.second) +
                    shaderEnumToString.find(it.first)->second + "\n" + buf.data());
        }

        glAttachShader(programID, id);
    }

    /*
        Link program
    */
    glLinkProgram(programID);
    GLint isLinked = 0;
    glGetProgramiv(programID, GL_LINK_STATUS, (int*)&isLinked);
    if (isLinked == GL_FALSE) {
        GLint maxLength = 0;
        glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &maxLength);
        std::vector<char> buf(maxLength);
        glGetProgramInfoLog(
            programID, maxLength, &maxLength, reinterpret_cast<GLchar*>(buf.data()));

        failFun("Link error:\n" + annotateLines(wholeShader) + "\n" + buf.data());
    }

    for (auto it : shaderIDs) glDeleteShader(it.second);

    /*
        Extract attribute and uniform locations
    */
    auto getVars = [](GLint id, GLenum varEnum) {
        const int MAX_VAR_LEN = 256;

        glUseProgram(id);
        // std::unordered_map<std::string, int> vars;
        std::unordered_map<std::string, ShaderValue> resources;

        int n;
        glGetProgramiv(id, varEnum, &n);
        char name[MAX_VAR_LEN];

        for (auto i = 0; i < n; i++) {
            ShaderValue sr;

            int nameLen;
            if (varEnum == GL_ACTIVE_UNIFORMS)
                glGetActiveUniform(id, i, MAX_VAR_LEN, &nameLen, &sr.size, &sr.type, name);
            else
                glGetActiveAttrib(id, i, MAX_VAR_LEN, &nameLen, &sr.size, &sr.type, name);

            name[nameLen] = 0;
            if (nameLen > 3 && strcmp(name + nameLen - 3, "[0]") == 0) name[nameLen - 3] = 0;

            if (varEnum == GL_ACTIVE_UNIFORMS) {
                sr.location = glGetUniformLocation(id, name);
                sr.shaderInterface = ShaderInterface::SHADER_INTERFACE_UNIFORM;
            } else {
                sr.location = glGetAttribLocation(id, name);
                sr.shaderInterface = ShaderInterface::SHADER_INTERFACE_ATTRIB;
            }

            resources[name] = sr;
        }

        glUseProgram(0);
        return resources;
    };

    auto attribs = getVars(programID, GL_ACTIVE_ATTRIBUTES);
    auto uniforms = getVars(programID, GL_ACTIVE_UNIFORMS);
    attribs.insert(uniforms.begin(), uniforms.end());

    m_resources = std::move(attribs);
    m_id = programID;
}

ShaderValue& Shader::operator[](const std::string& name)
{
    auto it = m_resources.find(name);

    // Not found, return none type
    if (it == m_resources.end()) {
        return ShaderValue::none;
    }

    return it->second;
}

bool Shader::bind() const
{
    GL(glUseProgram(m_id));
    return true;
}

void Shader::unbind()
{
    GL(glUseProgram(0));
}

const std::string& Shader::get_source() const
{
    return m_source;
}

std::string& Shader::get_source()
{
    return m_source;
}

const ShaderDefines& Shader::get_defines() const
{
    return m_defines;
}


ShaderValue& ShaderValue::operator=(const std::vector<Eigen::Vector3f>& arr)
{
    if (location == -1) return *this;

    if (shaderInterface == SHADER_INTERFACE_UNIFORM) {
        GL(glUniform3fv(
            location, static_cast<int>(arr.size()), reinterpret_cast<const GLfloat*>(arr.data())));
    } else {
        assert(false);
    }

    return *this;
}


ShaderValue& ShaderValue::set_array(const float* data, int n)
{
    if (location == -1) return *this;

    if (shaderInterface == SHADER_INTERFACE_UNIFORM) {
        GL(glUniform1fv(location, n, reinterpret_cast<const GLfloat*>(data)));
    } else {
        assert(false);
    }

    return *this;
}

ShaderValue& ShaderValue::set_array(const int* data, int n)
{
    if (location == -1) return *this;

    if (shaderInterface == SHADER_INTERFACE_UNIFORM) {
        GL(glUniform1iv(location, n, reinterpret_cast<const GLint*>(data)));
    } else {
        assert(false);
    }

    return *this;
}

ShaderValue& ShaderValue::set_array(const unsigned int* data, int n)
{
    if (location == -1) return *this;

    if (shaderInterface == SHADER_INTERFACE_UNIFORM) {
        GL(glUniform1uiv(location, n, reinterpret_cast<const GLuint*>(data)));
    } else {
        assert(false);
    }

    return *this;
}


ShaderValue& ShaderValue::set_vectors(const Eigen::Vector2f* data, int n)
{
    if (location == -1) return *this;

    if (shaderInterface == SHADER_INTERFACE_UNIFORM) {
        GL(glUniform2fv(location, n, reinterpret_cast<const GLfloat*>(data)));
    } else {
        assert(false);
    }

    return *this;
}

ShaderValue& ShaderValue::set_vectors(const Eigen::Vector3f* data, int n)
{
    if (location == -1) return *this;

    if (shaderInterface == SHADER_INTERFACE_UNIFORM) {
        GL(glUniform3fv(location, n, reinterpret_cast<const GLfloat*>(data)));
    } else {
        assert(false);
    }
    return *this;
}

ShaderValue& ShaderValue::set_vectors(const Eigen::Vector4f* data, int n)
{
    if (location == -1) return *this;

    if (shaderInterface == SHADER_INTERFACE_UNIFORM) {
        GL(glUniform4fv(location, n, reinterpret_cast<const GLfloat*>(data)));
    } else {
        assert(false);
    }

    return *this;
}

ShaderValue& ShaderValue::set_matrices(
    const Eigen::Matrix2f* data, int n, bool transpose /*= false*/)
{
    if (location == -1) return *this;

    if (shaderInterface == SHADER_INTERFACE_UNIFORM) {
        GL(glUniformMatrix2fv(
            location, n, transpose ? GL_TRUE : GL_FALSE, reinterpret_cast<const GLfloat*>(data)));
    } else {
        assert(false);
    }

    return *this;
}

ShaderValue& ShaderValue::set_matrices(
    const Eigen::Matrix3f* data, int n, bool transpose /*= false*/)
{
    if (location == -1) return *this;

    if (shaderInterface == SHADER_INTERFACE_UNIFORM) {
        GL(glUniformMatrix3fv(
            location, n, transpose ? GL_TRUE : GL_FALSE, reinterpret_cast<const GLfloat*>(data)));
    } else {
        assert(false);
    }

    return *this;
}

ShaderValue& ShaderValue::set_matrices(
    const Eigen::Matrix4f* data, int n, bool transpose /*= false*/)
{
    if (location == -1) return *this;

    if (shaderInterface == SHADER_INTERFACE_UNIFORM) {
        GL(glUniformMatrix4fv(
            location, n, transpose ? GL_TRUE : GL_FALSE, reinterpret_cast<const GLfloat*>(data)));
    } else {
        assert(false);
    }

    return *this;
}

ShaderValue& ShaderValue::set_matrices(
    const Eigen::Affine3f* data, int n, bool transpose /*= false*/)
{
    // defer to Matrix4f
    std::vector<Eigen::Matrix4f> data_mat4;
    for (int i = 0; i < n; i++) {
        data_mat4[i] = data[i].matrix();
    }
    return set_matrices(data_mat4.data(), n, transpose);
}

ShaderValue ShaderValue::none = {-1, 0, 0, SHADER_INTERFACE_NONE};


ShaderValue& ShaderValue::operator=(Eigen::Vector2f val)
{
    if (location == -1) return *this;
    assert(type == GL_FLOAT_VEC2);

    if (shaderInterface == SHADER_INTERFACE_UNIFORM) {
        GL(glUniform2fv(location, 1, val.data()));
    } else {
        GL(glVertexAttrib2fv(location, val.data()));
    }

    return *this;
}

ShaderValue& ShaderValue::operator=(Eigen::Vector3f val)
{
    if (location == -1) return *this;
    assert(type == GL_FLOAT_VEC3);

    if (shaderInterface == SHADER_INTERFACE_UNIFORM) {
        GL(glUniform3fv(location, 1, val.data()));
    } else {
        GL(glVertexAttrib3fv(location, val.data()));
    }

    return *this;
}

ShaderValue& ShaderValue::operator=(Eigen::Vector4f val)
{
    if (location == -1) return *this;
    assert(type == GL_FLOAT_VEC4);

    if (shaderInterface == SHADER_INTERFACE_UNIFORM) {
        GL(glUniform4fv(location, 1, val.data()));
    } else {
        GL(glVertexAttrib4fv(location, val.data()));
    }

    return *this;
}

ShaderValue& ShaderValue::operator=(Eigen::Matrix2f val)
{
    if (location == -1) return *this;
    assert(type == GL_FLOAT_MAT2 && shaderInterface == SHADER_INTERFACE_UNIFORM);
    GL(glUniformMatrix2fv(location, 1, GL_FALSE, val.data()));
    return *this;
}

ShaderValue& ShaderValue::operator=(Eigen::Matrix3f val)
{
    if (location == -1) return *this;
    assert(type == GL_FLOAT_MAT3 && shaderInterface == SHADER_INTERFACE_UNIFORM);
    GL(glUniformMatrix3fv(location, 1, GL_FALSE, val.data()));
    return *this;
}

ShaderValue& ShaderValue::operator=(Eigen::Matrix4f val)
{
    if (location == -1) return *this;
    assert(type == GL_FLOAT_MAT4 && shaderInterface == SHADER_INTERFACE_UNIFORM);
    GL(glUniformMatrix4fv(location, 1, GL_FALSE, val.data()));
    return *this;
}

ShaderValue& ShaderValue::operator=(Eigen::Affine3f val)
{
    // defer to Matrix4f
    return ((*this) = val.matrix());
}

ShaderValue& ShaderValue::operator=(Eigen::Projective3f val)
{
    // defer to Matrix4f
    return ((*this) = val.matrix());
}


ShaderValue& ShaderValue::operator=(double val)
{
    if (location == -1) return *this;
    assert(type == GL_DOUBLE);

    if (shaderInterface == SHADER_INTERFACE_UNIFORM) {
        GL(glUniform1d(location, val));
    } else {
        GL(glVertexAttrib1d(location, val));
    }

    return *this;
}

ShaderValue& ShaderValue::operator=(float val)
{
    if (location == -1) return *this;
    assert(type == GL_FLOAT);

    if (shaderInterface == SHADER_INTERFACE_UNIFORM) {
        GL(glUniform1f(location, val));
    } else {
        GL(glVertexAttrib1f(location, val));
    }

    return *this;
}

ShaderValue& ShaderValue::operator=(int val)
{
    if (location == -1) return *this;

    if (shaderInterface == SHADER_INTERFACE_UNIFORM) {
        GL(glUniform1i(location, val));
    } else {
        GL(glVertexAttribI1i(location, val));
    }

    return *this;
}

ShaderValue& ShaderValue::operator=(bool val)
{
    if (location == -1) return *this;
    assert(type == GL_BOOL);

    if (shaderInterface == SHADER_INTERFACE_UNIFORM) {
        GL(glUniform1i(location, val));
    } else {
        GL(glVertexAttribI1i(location, val));
    }

    return *this;
}

} // namespace ui
} // namespace lagrange

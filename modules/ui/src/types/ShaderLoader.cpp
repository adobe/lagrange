/*
 * Copyright 2021 Adobe. All rights reserved.
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
#include <lagrange/fs/file_utils.h>
#include <lagrange/ui/types/ShaderLoader.h>

#ifndef DEFAULT_SHADERS_USE_REAL_PATH
// Contains virtual fs with shaders: VIRTUAL_SHADERS
#include <shaders.inc>
#endif


namespace lagrange {
namespace ui {

entt::id_type hash_shader_definition(const ShaderDefinition& def)
{
    std::string hash_str = def.path + (def.path_type == ShaderLoader::PathType::REAL ? "0" : "1");
    for (const auto& d : def.defines) {
        hash_str += d.first + d.second;
    }
    return entt::hashed_string(hash_str.c_str());
}


std::shared_ptr<Shader> ShaderLoader::load(
    const std::string& generic_path,
    PathType pathtype,
    const ShaderDefines& defines /*= {}*/) const
{
    try {
        if (pathtype == PathType::REAL) {
            return std::make_shared<Shader>(fs::read_file_with_includes("", generic_path), defines);
        }

#ifdef DEFAULT_SHADERS_USE_REAL_PATH
        return std::make_shared<Shader>(
            fs::read_file_with_includes(std::string(DEFAULT_SHADERS_USE_REAL_PATH), generic_path),
            defines);
#else
        return std::make_shared<Shader>(
            fs::read_file_with_includes(generic_path, VIRTUAL_SHADERS),
            defines);
#endif
    } catch (const ShaderException& ex) {
        lagrange::logger().error("{}\nin {}", ex.what(), generic_path);

#if defined(__EMSCRIPTEN__)

        static std::string fallback_shader = "#pragma VERTEX"
                                             "in vec3 in_pos;"
                                             "uniform mat4 PV;"
                                             "uniform mat4 M;"
                                             "void main() {"
                                             "    gl_Position = PV * M * vec4(in_pos, 1);"
                                             "}"
                                             "#pragma FRAGMENT"
                                             "out vec4 fragColor;"
                                             "precision mediump float;"
                                             "void main() {"
                                             "    fragColor = vec4(1, 0, 0, 1);"
                                             "}";
#else

        static std::string fallback_shader = "#pragma VERTEX"
                                             "in vec3 in_pos;"
                                             "uniform mat4 PV;"
                                             "uniform mat4 M;"
                                             "void main() {"
                                             "    gl_Position = PV * M * vec4(in_pos, 1);"
                                             "}"
                                             "#pragma FRAGMENT"
                                             "out vec4 fragColor;"
                                             "void main() {"
                                             "    fragColor = vec4(1, 0, 0, 1);"
                                             "}";


#endif

        try {
            return std::make_shared<Shader>(fallback_shader, ShaderDefines{});
        } catch (const ShaderException& ex) {
            lagrange::logger().error("Failed to compile fallback shader: {}", ex.what());
        }
    }
    return nullptr;
}

entt::id_type
register_shader_variant(Registry& r, entt::id_type id, const ShaderDefines& shader_defines)
{
    if (shader_defines.size() == 0) return id;

    auto& registered = r.ctx_or_set<RegisteredShaders>();
    auto def_it = registered.find(id);
    if (def_it == registered.end()) return entt::null;

    // Add to existing defines
    ShaderDefinition new_sdef = def_it->second;
    for (auto& define : shader_defines) {
        new_sdef.defines.push_back(define);
    }

    // Caculate alternative cache key
    auto alternate_id = hash_shader_definition(new_sdef);
    return register_shader_as(r, alternate_id, new_sdef);
}


ShaderResource get_shader(Registry& registry, entt::id_type id)
{
    auto& cache = registry.ctx_or_set<ShaderCache>();

    if (cache.contains(id)) return cache.handle(id);

    // Shader not registered, return empty handle
    auto& registered = registry.ctx_or_set<RegisteredShaders>();
    auto def_it = registered.find(id);
    if (def_it == registered.end()) return ShaderResource();

    return cache.load<ShaderLoader>(
        id,
        def_it->second.path,
        def_it->second.path_type,
        def_it->second.defines);
}

RegisteredShaders& get_registered_shaders(Registry& registry)
{
    return registry.ctx_or_set<RegisteredShaders>();
}

ShaderCache& get_shader_cache(Registry& registry)
{
    return registry.ctx_or_set<ShaderCache>();
}

bool add_file_to_shader_virtual_fs(
    const std::string& virtual_path,
    const std::string& contents,
    bool overwrite)
{
#ifndef DEFAULT_SHADERS_USE_REAL_PATH
    if (!overwrite) {
        auto res = VIRTUAL_SHADERS.insert({virtual_path, contents});
        return res.second;
    } else {
        VIRTUAL_SHADERS[virtual_path] = contents;
        return true;
    }
#else
    return false;
#endif
}

entt::id_type register_shader_as(Registry& registry, entt::id_type id, const ShaderDefinition& def)
{
    auto& registered = registry.ctx_or_set<RegisteredShaders>();
    registered[id] = def;
    return id;
}


entt::id_type register_shader(Registry& registry, const ShaderDefinition& def)
{
    return register_shader_as(registry, hash_shader_definition(def), def);
}

entt::id_type
register_shader(Registry& registry, const std::string& path, const std::string& display_name)
{
    ShaderDefinition def;
    def.path = path;
    def.display_name = display_name;
    return register_shader(registry, def);
}


} // namespace ui
} // namespace lagrange
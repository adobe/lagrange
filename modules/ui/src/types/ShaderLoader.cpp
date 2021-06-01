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
#include <lagrange/fs/file_utils.h>
#include <lagrange/ui/types/ShaderLoader.h>

#ifndef DEFAULT_SHADERS_USE_REAL_PATH
// Contains virtual fs with shaders: VIRTUAL_SHADERS
#include <shaders.inc>
#endif


namespace lagrange {
namespace ui {


std::shared_ptr<Shader> ShaderLoader::load(
    const std::string& generic_path,
    PathType pathtype,
    ShaderDefines defines /*= {}*/) const
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
    }
    return nullptr;
}


ShaderResource get_shader(Registry& registry, entt::id_type id)
{
    auto& cache = registry.ctx_or_set<ShaderCache>();
    if (cache.contains(id)) return cache.handle(id);

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

entt::id_type register_shader_as(Registry& registry, entt::id_type id, const ShaderDefinition& def)
{
    auto& registered = registry.ctx_or_set<RegisteredShaders>();
    registered[id] = def;
    return id;
}

entt::id_type register_shader(Registry& registry, const ShaderDefinition& def)
{
    std::string hash_str = def.path + (def.path_type == ShaderLoader::PathType::REAL ? "0" : "1");
    for (const auto& d : def.defines) {
        hash_str += d.first + d.second;
    }

    return register_shader_as(registry, entt::hashed_string(hash_str.c_str()), def);
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
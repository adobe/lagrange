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
#pragma once

#include <lagrange/ui/Entity.h>
#include <lagrange/ui/types/Shader.h>

namespace lagrange {
namespace ui {

struct ShaderLoader : entt::resource_loader<ShaderLoader, Shader>
{
    enum class PathType { REAL, VIRTUAL };

    std::shared_ptr<Shader> load(
        const std::string& generic_path,
        PathType pathtype,
        const ShaderDefines& defines = {}) const;
};
using ShaderCache = entt::resource_cache<Shader>;
using ShaderResource = entt::resource_handle<Shader>;

// using ShaderDef = std::tuple<std::string, std::string, ShaderLoader::PathType, ShaderDefines>;
struct ShaderDefinition
{
    std::string path;
    ShaderLoader::PathType path_type = ShaderLoader::PathType::REAL;
    std::string display_name;
    ShaderDefines defines;
};
using RegisteredShaders = std::unordered_map<entt::id_type, ShaderDefinition>;


//Register shader with given id that can be used to load/reload and access the shader
entt::id_type register_shader_as(Registry& r, entt::id_type id, const ShaderDefinition& def);

//Register shader, returns and ID that can be used to load/reload and access the shader
entt::id_type register_shader(Registry& r, const ShaderDefinition& def);
entt::id_type
register_shader(Registry& r, const std::string& path, const std::string& display_name);

entt::id_type
register_shader_variant(Registry& r, entt::id_type id, const ShaderDefines& shader_defines);

ShaderResource get_shader(Registry& r, entt::id_type id);


RegisteredShaders& get_registered_shaders(Registry& r);
ShaderCache& get_shader_cache(Registry& r);

/// Creates a file using `virtual_path` with `contents` in the shader virtual file system.
/// This file will be visible to the ShaderLoader, to be directly loaded as a shader
/// or to be included in another shader via #include "virtual/fs/path/.."
/// Returns true if written successfully.
/// Returns false if 1. No virtual fs is used (DEFAULT_SHADERS_USE_REAL_PATH is defined), 2. File already exists and overwrite==false
bool add_file_to_shader_virtual_fs(
    const std::string& virtual_path,
    const std::string& contents,
    bool overwrite = false);

} // namespace ui
} // namespace lagrange
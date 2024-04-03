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

#include <lagrange/fs/api.h>
#include <lagrange/fs/filesystem.h>

#include <string>
#include <unordered_map>

namespace lagrange {
namespace fs {

/// Helpers for when you don't want to use fs::path type.
/// (You should use fs::path instead and avoid calling these)
LA_FS_API std::string get_filename_extension(const std::string& filename);

LA_FS_API std::string get_base_dir(const std::string& filename);

/// Ensures that the string ends with suffix. Will append suffix if needed.
LA_FS_API std::string get_string_ending_with(const std::string& str, const char* suffix);

LA_FS_API std::string read_file_to_string(const path& path);
LA_FS_API std::string read_file_with_includes(const path& search_dir, const path& filepath);
LA_FS_API std::string read_file_with_includes(
    const path& filepath,
    const std::unordered_map<std::string, std::string>& virtual_fs);

// The executable is not present in WebAssembly's virtual file system, so it does not make sense
// to provide get_executable_path() or get_executable_directory() when building with Emscripten.
#if !defined(__EMSCRIPTEN__)
LA_FS_API path get_executable_path();

LA_FS_API path get_executable_directory();
#endif

/// cwd
LA_FS_API path get_current_working_directory();

} // namespace fs
} // namespace lagrange

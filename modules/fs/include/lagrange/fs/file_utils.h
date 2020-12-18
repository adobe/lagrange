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

#include <string>
#include <unordered_map>

#include <lagrange/fs/filesystem.h>


namespace lagrange {
namespace fs {

/// Helpers for when you don't want to use fs::path type.
/// (You should use fs::path instead and avoid calling these)
std::string get_filename_extension(const std::string& filename);

std::string get_base_dir(const std::string& filename);

/// Ensures that the string ends with suffix. Will append suffix if needed.
std::string get_string_ending_with(const std::string& str, const char* suffix);

std::string read_file_to_string(const path& path);
std::string read_file_with_includes(const path& search_dir, const path& filepath);
std::string read_file_with_includes(
    const path& filepath, const std::unordered_map<std::string, std::string>& virtual_fs);

path get_executable_path();

path get_executable_directory();

/// cwd
path get_current_working_directory();

} // namespace fs
} // namespace lagrange

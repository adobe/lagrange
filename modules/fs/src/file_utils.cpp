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
#include <lagrange/utils/strings.h>
#include <lagrange/fs/file_utils.h>

#include <lagrange/utils/assert.h>

#ifdef _WIN32
#include <Windows.h>
#elif __APPLE__
#include <mach-o/dyld.h>
#include <unistd.h>
#elif __linux__ || __EMSCRIPTEN__
#include <unistd.h>
#else
#error Platform not supported
#endif

#include <fstream>
#include <regex>

namespace lagrange {
namespace fs {

std::string get_filename_extension(const std::string& filename)
{
    return path(filename).extension().string();
}

std::string get_base_dir(const std::string& filename)
{
    return path(filename).parent_path().string();
}

std::string get_string_ending_with(const std::string& str, const char* suffix)
{
    if (ends_with(str, suffix)) {
        return std::string(str);
    } else {
        return std::string(str) + std::string(suffix);
    }
}

std::string read_file_to_string(const path& filepath)
{
    std::string str;

    fs::ifstream f(filepath, std::ios::binary | std::ios::ate);
    if (!f.good()) return str;

    str.reserve(f.tellg());
    f.seekg(0, std::ios::beg);
    str.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return str;
}


std::string read_file_with_includes(const fs::path& search_dir, const fs::path& filepath)
{
    const fs::path absolute_filepath = search_dir.empty() ? filepath : (search_dir / filepath);

    std::string str = lagrange::fs::read_file_to_string(absolute_filepath);

    std::regex rx_include("#include +\"(.*)\"");

    auto rx_begin = std::sregex_iterator(str.begin(), str.end(), rx_include);
    auto rx_end = std::sregex_iterator();

    for (auto i = rx_begin; i != rx_end;) {
        if (i->size() > 1) {
            auto include_file_name = (*i)[1].str();
            fs::path include_file_path = search_dir / include_file_name;

            std::string new_src = lagrange::fs::read_file_to_string(include_file_path);
            la_runtime_assert(new_src.length() > 0, "Couldn't read " + include_file_path.string());

            str.replace(i->position(), i->length(), new_src);
            i = std::sregex_iterator(str.begin(), str.end(), rx_include);
        } else {
            ++i;
        }
    }

    return str;
}

std::string read_file_with_includes(
    const fs::path& filepath, const std::unordered_map<std::string, std::string>& virtual_fs)
{
    auto it = virtual_fs.find(filepath.generic_string());
    if (it == virtual_fs.end())
        la_runtime_assert(false, filepath.string() + " is not in virtual file system");

    // make a copy
    auto str = (*it).second;

    std::regex rx_include("#include +\"(.*)\"");

    auto rx_begin = std::sregex_iterator(str.begin(), str.end(), rx_include);
    auto rx_end = std::sregex_iterator();

    for (auto i = rx_begin; i != rx_end;) {
        if (i->size() > 1) {
            auto include_file_name = (*i)[1].str();

            auto it_include = virtual_fs.find(include_file_name);
            if (it_include == virtual_fs.end())
                la_runtime_assert(
                    false, "#include of " + include_file_name + " is not in virtual file system");

            const auto& new_src = (*it_include).second;
            str.replace(i->position(), i->length(), new_src);

            // Rerun regex to discover more includes
            i = std::sregex_iterator(str.begin(), str.end(), rx_include);
        } else {
            ++i;
        }
    }

    return str;
}

#if !defined(__EMSCRIPTEN__)

path get_executable_path()
{
    constexpr short BUF_SIZE = 2048;
    path result;

#ifdef _WIN32
    // Use the A postfix. It would return 8bit ANSI string.
    // Using W postfix will return 16 bit windows UTF (Yuk!) strings
    // Using no postfil apparently depends on whether `UNICODE` is defined.
    // (which is the case for example in dimension)
    // See https://stackoverflow.com/questions/12460712/convert-lpwstr-to-string for more
    // information.
    // The ANSI encoding won't handle fancy file names with unicode in it, but it should
    // be enough for our use cases.
    // Unfortunately, the wonderful windows API does not return strings in utf-8 encoding.
    char path[BUF_SIZE] = {0};
    GetModuleFileNameA(NULL, path, BUF_SIZE);
    result = path;
#elif __APPLE__
    char path[BUF_SIZE];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        result = path;
    } else {
        result = ".";
    }
#elif __linux__
    char dest[BUF_SIZE];
    memset(dest, 0, sizeof(dest));
    auto count = readlink("/proc/self/exe", dest, BUF_SIZE);
    la_runtime_assert(count >= 0);
    result = dest;
#endif

    return result.lexically_normal();
}

path get_executable_directory()
{
    return get_executable_path().parent_path();
}

#endif // !defined(__EMSCRIPTEN__)

path get_current_working_directory()
{
    constexpr short BUF_SIZE = 2048;
#ifdef _WIN32
    // Use the A postfix. It would return 8bit Unicode
    char buffer[BUF_SIZE];
    auto ret = GetCurrentDirectoryA(BUF_SIZE, buffer);
    la_runtime_assert(ret != 0);
    return buffer;
#elif __APPLE__ || __linux__ || __EMSCRIPTEN__
    char buffer[BUF_SIZE];
    char* success = getcwd(buffer, BUF_SIZE);
    la_runtime_assert(success);
    return buffer;
#endif
}

} // namespace fs
} // namespace lagrange

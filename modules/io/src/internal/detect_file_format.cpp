/*
 * Copyright 2023 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/io/internal/detect_file_format.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/strings.h>

#include <algorithm>
#include <string_view>

namespace lagrange::io::internal {

namespace {

// Returns true if the header window looks like a PCD file: skipping blank and comment lines, the
// first meaningful line is the canonical "# .PCD" banner or begins with a PCD header keyword.
bool looks_like_pcd(std::string_view window)
{
    while (!window.empty()) {
        const size_t end = window.find_first_of("\r\n");
        std::string_view line = window.substr(0, end);
        window.remove_prefix(end == std::string_view::npos ? window.size() : end + 1);

        const size_t start = line.find_first_not_of(" \t");
        if (start == std::string_view::npos) continue;
        line.remove_prefix(start);
        if (starts_with(line, "# .PCD")) return true;
        if (line[0] == '#') continue; // Other comment lines are allowed before the header.

        // First meaningful line decides: PCD headers start with VERSION/FIELDS/COLUMNS.
        return starts_with(line, "VERSION") || starts_with(line, "FIELDS") ||
               starts_with(line, "COLUMNS");
    }
    return false;
}

} // namespace

FileFormat detect_file_format(std::istream& input_stream)
{
    if (input_stream.peek() == EOF) {
        return FileFormat::Unknown;
    }
    la_runtime_assert(input_stream.good(), "Input stream is not good.");

    // Read a header window (restoring the stream position) so we can look past leading comments.
    auto pos = input_stream.tellg();
    char buffer[1024];
    input_stream.read(buffer, sizeof(buffer));
    const std::streamsize count = input_stream.gcount();
    input_stream.clear();
    input_stream.seekg(pos);
    std::string_view window(buffer, static_cast<size_t>(count));
    std::string_view header = window.substr(0, std::min<size_t>(window.size(), 5));

    if (starts_with(header, "glTF")) {
        return FileFormat::Gltf;
    } else if (starts_with(header, "{")) {
        return FileFormat::Gltf;
    } else if (starts_with(header, "ply")) {
        return FileFormat::Ply;
    } else if (starts_with(header, "$Mesh")) {
        return FileFormat::Msh;
    } else if (starts_with(header, "Kayda")) {
        // FBX binary header starts with "Kaydara FBX Binary".
        return FileFormat::Fbx;
    } else if (starts_with(header, "solid")) {
        return FileFormat::Stl;
    } else if (looks_like_pcd(window)) {
        // PCD files may begin with arbitrary comment lines before the VERSION/FIELDS header.
        return FileFormat::Pcd;
    } else {
        for (auto& flag : {"v", "f", "o", "u", "s", "g", "#"}) {
            if (starts_with(header, flag)) {
                return FileFormat::Obj;
            }
        }
        return FileFormat::Unknown;
    }
}

} // namespace lagrange::io::internal

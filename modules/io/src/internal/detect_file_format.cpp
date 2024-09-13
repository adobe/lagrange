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

#include <string_view>

namespace lagrange::io::internal {

FileFormat detect_file_format(std::istream& input_stream)
{
    if (input_stream.peek() == EOF) {
        return FileFormat::Unknown;
    }
    la_runtime_assert(input_stream.good(), "Input stream is not good.");

    // Extract file header.
    auto pos = input_stream.tellg();
    char header[5];
    input_stream.read(header, 5);
    input_stream.seekg(pos);
    std::string_view header_str(header, 5);

    if (starts_with(header_str, "glTF")) {
        return FileFormat::Gltf;
    } else if (starts_with(header_str, "{")) {
        return FileFormat::Gltf;
    } else if (starts_with(header_str, "ply")) {
        return FileFormat::Ply;
    } else if (starts_with(header_str, "$Mesh")) {
        return FileFormat::Msh;
    } else if (starts_with(header_str, "Kayda")) {
        // FBX binary header starts with "Kaydara FBX Binary".
        return FileFormat::Fbx;
    } else {
        for (auto& flag : {"v", "f", "o", "u", "s", "g", "#"}) {
            if (starts_with(header_str, flag)) {
                return FileFormat::Obj;
            }
        }
        return FileFormat::Unknown;
    }
}

} // namespace lagrange::io::internal

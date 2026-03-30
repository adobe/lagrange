/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include "detect_type.h"

#include "CistaMesh.h"
#include "CistaScene.h"
#include "CistaSimpleScene.h"
#include "compress.h"

#include <lagrange/utils/assert.h>

#include <cista/type_hash/type_hash.h>

#include <cstring>

namespace lagrange::serialization::internal {

std::vector<uint8_t> read_file_to_buffer(const fs::path& filename)
{
    fs::ifstream ifs(filename, std::ios::binary | std::ios::ate);
    la_runtime_assert(ifs.good(), "Failed to open file for reading: " + filename.string());
    auto size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(static_cast<size_t>(size));
    ifs.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(size));
    la_runtime_assert(ifs.good(), "Failed to read from file: " + filename.string());
    return buf;
}

std::vector<uint8_t> ensure_decompressed(span<const uint8_t> buffer)
{
    if (is_compressed(buffer)) {
        return decompress_buffer(buffer);
    }
    return std::vector<uint8_t>(buffer.begin(), buffer.end());
}

EncodedType detect_encoded_type(span<const uint8_t> buffer)
{
    // With WITH_VERSION mode, cista stores the type hash in the first 8 bytes of the buffer.
    if (buffer.size() < sizeof(cista::hash_t)) {
        return EncodedType::Unknown;
    }

    cista::hash_t stored_hash = 0;
    std::memcpy(&stored_hash, buffer.data(), sizeof(cista::hash_t));

    static const cista::hash_t mesh_hash = cista::type_hash<CistaMesh>();
    static const cista::hash_t simple_scene_hash = cista::type_hash<CistaSimpleScene>();
    static const cista::hash_t scene_hash = cista::type_hash<CistaScene>();

    if (stored_hash == mesh_hash) return EncodedType::Mesh;
    if (stored_hash == simple_scene_hash) return EncodedType::SimpleScene;
    if (stored_hash == scene_hash) return EncodedType::Scene;

    return EncodedType::Unknown;
}

} // namespace lagrange::serialization::internal

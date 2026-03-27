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
#pragma once

#include <lagrange/fs/filesystem.h>
#include <lagrange/utils/span.h>

#include <cstdint>
#include <vector>

namespace lagrange::serialization::internal {

/// Type of data stored in an encoded buffer.
enum class EncodedType {
    Mesh,
    SimpleScene,
    Scene,
    Unknown,
};

/// Read a file into a byte buffer.
std::vector<uint8_t> read_file_to_buffer(const fs::path& filename);

/// Decompress an encoded buffer if compressed. Returns the original buffer if not compressed.
std::vector<uint8_t> ensure_decompressed(span<const uint8_t> buffer);

/// Detect the type of data stored in an encoded (and already decompressed) buffer.
/// The buffer must already be decompressed.
EncodedType detect_encoded_type(span<const uint8_t> buffer);

} // namespace lagrange::serialization::internal

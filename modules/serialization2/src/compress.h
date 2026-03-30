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

#include <lagrange/utils/span.h>

#include <cista/serialization.h>

#include <cstdint>
#include <vector>

namespace lagrange::serialization::internal {

/// Cista serialization mode: type hash for version checking + integrity hash for corruption
/// detection.
constexpr auto k_cista_mode = cista::mode::WITH_VERSION | cista::mode::WITH_INTEGRITY;

/// Magic header for compressed buffers: "LENC" (Lagrange ENCoding).
constexpr uint8_t k_magic[4] = {'L', 'E', 'N', 'C'};

/// Header size: magic (4 bytes) + uncompressed size (8 bytes).
constexpr size_t k_header_size = 4 + 8;

/// Compress a buffer using zstd, prepending the LENC magic header.
///
/// @param[in]  input              The uncompressed data.
/// @param[in]  compression_level  Zstd compression level (1-22).
/// @param[in]  num_threads        Number of compression threads. 0 = automatic, 1 = single-threaded.
std::vector<uint8_t>
compress_buffer(const std::vector<uint8_t>& input, int compression_level, unsigned num_threads);

/// Decompress a buffer that was compressed with compress_buffer().
std::vector<uint8_t> decompress_buffer(span<const uint8_t> input);

/// Check if a buffer starts with the LENC magic header.
bool is_compressed(span<const uint8_t> buffer);

} // namespace lagrange::serialization::internal

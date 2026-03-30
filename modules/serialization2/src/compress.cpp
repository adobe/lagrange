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
#include "compress.h"

#include <lagrange/Logger.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/build.h>
#include <lagrange/utils/scope_guard.h>

#include <zstd.h>

#include <cstring>
#include <thread>

namespace lagrange::serialization::internal {

std::vector<uint8_t>
compress_buffer(const std::vector<uint8_t>& input, int compression_level, unsigned num_threads)
{
    const size_t max_compressed_size = ZSTD_compressBound(input.size());
    std::vector<uint8_t> output(k_header_size + max_compressed_size);

    // Write magic header
    std::memcpy(output.data(), k_magic, 4);

    // Write uncompressed size (little-endian uint64_t)
    uint64_t uncompressed_size = input.size();
    std::memcpy(output.data() + 4, &uncompressed_size, 8);

    // Compress using the advanced API to support multithreading
    ZSTD_CCtx* cctx = ZSTD_createCCtx();
    la_runtime_assert(cctx != nullptr, "Failed to create zstd compression context");
    auto cctx_guard = make_scope_guard([&]() noexcept { ZSTD_freeCCtx(cctx); });

    {
        size_t rc = ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, compression_level);
        la_runtime_assert(
            !ZSTD_isError(rc),
            "Failed to set zstd compression level: " + std::string(ZSTD_getErrorName(rc)));
    }

#if LAGRANGE_TARGET_COMPILER(EMSCRIPTEN)
    // On Emscripten with PROXY_TO_PTHREAD, multi-threaded zstd compression spawns additional worker
    // threads that can exhaust the fixed PTHREAD_POOL_SIZE under load, causing intermittent
    // "memory access out of bounds" crashes in the emscripten threading proxy layer (a_cas /
    // pthread_cond_signal / emscripten_proxy_finish). Force single-threaded compression.
    if (num_threads > 1) {
        logger().warn(
            "Ignoring multithreaded compression request on Emscripten. Falling back to "
            "single-threaded compression.");
    }
#else
    if (num_threads != 1) {
        unsigned workers = num_threads;
        if (workers == 0) {
            workers = std::thread::hardware_concurrency();
            if (workers == 0) {
                logger().warn(
                    "Failed to detect hardware concurrency, defaulting to 4 threads for "
                    "compression");
                workers = 4;
            }
        }
        size_t rc = ZSTD_CCtx_setParameter(cctx, ZSTD_c_nbWorkers, static_cast<int>(workers));
        la_runtime_assert(
            !ZSTD_isError(rc),
            "Failed to set zstd worker count: " + std::string(ZSTD_getErrorName(rc)));
    }
#endif

    const size_t compressed_size = ZSTD_compress2(
        cctx,
        output.data() + k_header_size,
        max_compressed_size,
        input.data(),
        input.size());

    la_runtime_assert(!ZSTD_isError(compressed_size), "Zstd compression failed");

    output.resize(k_header_size + compressed_size);
    return output;
}

std::vector<uint8_t> decompress_buffer(span<const uint8_t> input)
{
    la_runtime_assert(input.size() >= k_header_size, "Buffer too small for compressed header");

    // Read uncompressed size from our header
    uint64_t uncompressed_size = 0;
    std::memcpy(&uncompressed_size, input.data() + 4, 8);

    // Cross-check against zstd's frame content size to detect corrupted headers
    const uint8_t* zstd_data = input.data() + k_header_size;
    const size_t zstd_size = input.size() - k_header_size;
    const unsigned long long frame_size = ZSTD_getFrameContentSize(zstd_data, zstd_size);
    la_runtime_assert(
        frame_size != ZSTD_CONTENTSIZE_ERROR,
        "Invalid zstd frame in compressed buffer");
    if (frame_size != ZSTD_CONTENTSIZE_UNKNOWN) {
        la_runtime_assert(
            uncompressed_size == frame_size,
            "Header uncompressed size (" + std::to_string(uncompressed_size) +
                ") does not match zstd frame content size (" + std::to_string(frame_size) + ")");
    }

    std::vector<uint8_t> output(uncompressed_size);

    const size_t result = ZSTD_decompress(
        output.data(),
        uncompressed_size,
        input.data() + k_header_size,
        input.size() - k_header_size);

    la_runtime_assert(!ZSTD_isError(result), "Zstd decompression failed");
    la_runtime_assert(result == uncompressed_size, "Zstd decompressed size mismatch");

    return output;
}

bool is_compressed(span<const uint8_t> buffer)
{
    if (buffer.size() < k_header_size) return false;
    return std::memcmp(buffer.data(), k_magic, 4) == 0;
}

} // namespace lagrange::serialization::internal

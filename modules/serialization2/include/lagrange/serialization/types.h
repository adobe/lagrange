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

#include <cstdint>

namespace lagrange::serialization {

///
/// Options for serialization (save/serialize functions).
///
struct SerializeOptions
{
    /// Enable zstd compression.
    bool compress = true;

    /// Zstd compression level (1-22). Lower is faster, higher is better compression. Levels >= 20
    /// require more memory and should be used with caution.
    int compression_level = 3;

    /// Number of zstd compression threads. 0 = automatic, 1 = single-threaded. >1 = passed to
    /// ZSTD_c_nbWorkers. On Emscripten, this setting is ignored (always single-threaded).
    unsigned num_threads = 0;
};

///
/// Options for deserialization (load/deserialize functions).
///
struct DeserializeOptions
{
    ///
    /// Allow converting between meshes and scenes during deserialization.
    ///
    /// When enabled, deserializing a buffer that contains a different type than requested will
    /// attempt automatic conversion. For example, calling deserialize_mesh() on a buffer containing
    /// a Scene will convert the scene to a single mesh using scene_to_mesh().
    ///
    /// When disabled (the default), a type mismatch will throw an exception.
    ///
    /// @note A warning is logged when a conversion is performed, unless @ref quiet is set.
    ///
    bool allow_scene_conversion = false;

    ///
    /// Allow casting scalar and index types during deserialization.
    ///
    /// When enabled, deserializing a buffer serialized with different Scalar/Index types than the
    /// requested template parameters will cast the data accordingly. For example, deserializing a
    /// mesh saved as `<float, uint32_t>` into a `SurfaceMesh<double, uint64_t>`.
    ///
    /// This applies to all contained meshes, including those inside Scene and SimpleScene objects.
    /// For SimpleScene, instance transforms are also cast to the target scalar type.
    ///
    /// When disabled (the default), a scalar/index type mismatch will throw an exception.
    ///
    /// @note A warning is logged when a type mismatch is detected and casting is performed,
    ///       unless @ref quiet is set. If the stored types already match the requested types,
    ///       no casting or warning occurs regardless of this setting.
    ///
    bool allow_type_cast = false;

    ///
    /// Suppress warnings during deserialization.
    ///
    /// When set to true, suppresses the warnings that are normally logged when scene conversion
    /// (via @ref allow_scene_conversion) or type casting (via @ref allow_type_cast) is performed.
    ///
    bool quiet = false;
};

} // namespace lagrange::serialization

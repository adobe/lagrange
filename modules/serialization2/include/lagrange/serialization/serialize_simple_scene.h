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
#include <lagrange/scene/SimpleScene.h>
#include <lagrange/serialization/api.h>
#include <lagrange/serialization/types.h>
#include <lagrange/utils/span.h>

#include <cstdint>
#include <vector>

namespace lagrange::serialization {

/// @addtogroup group-serialization2
/// @{

///
/// Current simple scene serialization format version.
///
/// @return     The current simple scene serialization format version.
///
constexpr uint32_t simple_scene_format_version()
{
    return 1;
}

///
/// Serialize a SimpleScene to a byte buffer.
///
/// @param[in]  scene      The scene to serialize.
/// @param[in]  options    Serialization options (compression, etc.).
///
/// @tparam     Scalar     Mesh scalar type.
/// @tparam     Index      Mesh index type.
/// @tparam     Dimension  Scene dimension (2 or 3).
///
/// @return     A byte buffer containing the serialized scene.
///
template <typename Scalar, typename Index, size_t Dimension>
LA_SERIALIZATION2_API std::vector<uint8_t> serialize_simple_scene(
    const scene::SimpleScene<Scalar, Index, Dimension>& scene,
    const SerializeOptions& options = {});

///
/// Deserialize a SimpleScene from a byte buffer.
///
/// The function auto-detects whether the buffer is compressed. If the buffer contains a SurfaceMesh
/// or Scene instead of a SimpleScene, it can be automatically converted when
/// DeserializeOptions::allow_scene_conversion is enabled. If the buffer was serialized with
/// different Scalar/Index types, type casting can be enabled via
/// DeserializeOptions::allow_type_cast.
///
/// @param[in]  buffer     A byte buffer containing the serialized data.
/// @param[in]  options    Deserialization options.
///
/// @tparam     SceneType  SimpleScene type (e.g. scene::SimpleScene<float, uint32_t, 3>).
///
/// @return     The deserialized scene.
///
template <typename SceneType>
LA_SERIALIZATION2_API SceneType
deserialize_simple_scene(span<const uint8_t> buffer, const DeserializeOptions& options = {});

///
/// Save a SimpleScene to a file.
///
/// The suggested file extension is `.lgs`.
///
/// @param[in]  filename   Output file path.
/// @param[in]  scene      The scene to save.
/// @param[in]  options    Serialization options (compression, etc.).
///
/// @tparam     Scalar     Mesh scalar type.
/// @tparam     Index      Mesh index type.
/// @tparam     Dimension  Scene dimension (2 or 3).
///
template <typename Scalar, typename Index, size_t Dimension>
LA_SERIALIZATION2_API void save_simple_scene(
    const fs::path& filename,
    const scene::SimpleScene<Scalar, Index, Dimension>& scene,
    const SerializeOptions& options = {});

///
/// Load a SimpleScene from a file.
///
/// Reads the file into memory and calls deserialize_simple_scene(). The function auto-detects
/// whether the file contents are compressed.
///
/// @param[in]  filename   Input file path.
/// @param[in]  options    Deserialization options.
///
/// @tparam     SceneType  SimpleScene type (e.g. scene::SimpleScene<float, uint32_t, 3>).
///
/// @return     The loaded scene.
///
/// @see        deserialize_simple_scene() for details on scene conversion and type casting options.
///
template <typename SceneType>
LA_SERIALIZATION2_API SceneType
load_simple_scene(const fs::path& filename, const DeserializeOptions& options = {});

/// @}

} // namespace lagrange::serialization

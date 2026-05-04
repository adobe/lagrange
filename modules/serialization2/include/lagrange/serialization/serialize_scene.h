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
#include <lagrange/scene/Scene.h>
#include <lagrange/serialization/api.h>
#include <lagrange/serialization/types.h>
#include <lagrange/utils/span.h>

#include <cstdint>
#include <vector>

namespace lagrange::serialization {

/// @addtogroup group-serialization2
/// @{

///
/// Current scene serialization format version.
///
/// @return     The current scene serialization format version.
///
constexpr uint32_t scene_format_version()
{
    return 1;
}

///
/// Serialize a Scene to a byte buffer.
///
/// @param[in]  scene    The scene to serialize.
/// @param[in]  options  Serialization options (compression, etc.).
///
/// @tparam     Scalar   Mesh scalar type.
/// @tparam     Index    Mesh index type.
///
/// @return     A byte buffer containing the serialized scene.
///
template <typename Scalar, typename Index>
LA_SERIALIZATION2_API std::vector<uint8_t> serialize_scene(
    const scene::Scene<Scalar, Index>& scene,
    const SerializeOptions& options = {});

///
/// Deserialize a Scene from a byte buffer.
///
/// The function auto-detects whether the buffer is compressed. If the buffer contains a SurfaceMesh
/// or SimpleScene instead of a Scene, it can be automatically converted when
/// DeserializeOptions::allow_scene_conversion is enabled. If the buffer was serialized with
/// different Scalar/Index types, type casting can be enabled via
/// DeserializeOptions::allow_type_cast.
///
/// @param[in]  buffer     A byte buffer containing the serialized data.
/// @param[in]  options    Deserialization options.
///
/// @tparam     SceneType  Scene type (e.g. scene::Scene<float, uint32_t>).
///
/// @return     The deserialized scene.
///
template <typename SceneType>
LA_SERIALIZATION2_API SceneType
deserialize_scene(span<const uint8_t> buffer, const DeserializeOptions& options = {});

///
/// Save a Scene to a file.
///
/// The suggested file extension is `.lgs`.
///
/// @param[in]  filename  Output file path.
/// @param[in]  scene     The scene to save.
/// @param[in]  options   Serialization options (compression, etc.).
///
/// @tparam     Scalar    Mesh scalar type.
/// @tparam     Index     Mesh index type.
///
template <typename Scalar, typename Index>
LA_SERIALIZATION2_API void save_scene(
    const fs::path& filename,
    const scene::Scene<Scalar, Index>& scene,
    const SerializeOptions& options = {});

///
/// Load a Scene from a file.
///
/// Reads the file into memory and calls deserialize_scene(). The function auto-detects whether the
/// file contents are compressed.
///
/// @param[in]  filename   Input file path.
/// @param[in]  options    Deserialization options.
///
/// @tparam     SceneType  Scene type (e.g. scene::Scene<float, uint32_t>).
///
/// @return     The loaded scene.
///
/// @see        deserialize_scene() for details on scene conversion and type casting options.
///
template <typename SceneType>
LA_SERIALIZATION2_API SceneType
load_scene(const fs::path& filename, const DeserializeOptions& options = {});

/// @}

} // namespace lagrange::serialization

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

#include <lagrange/SurfaceMesh.h>
#include <lagrange/fs/filesystem.h>
#include <lagrange/serialization/api.h>
#include <lagrange/serialization/types.h>
#include <lagrange/utils/span.h>

#include <cstdint>
#include <vector>

namespace lagrange::serialization {

///
/// @defgroup   group-serialization2 Serialization
/// @ingroup    module-serialization2 Binary serialization of SurfaceMesh, SimpleScene and Scene objects.
///
/// The suggested file extension for serialized meshes is `.lgm`, and `.lgs` for serialized scenes
/// (both SimpleScene and Scene).
///
/// @{

///
/// Current mesh serialization format version.
///
/// @return     The current mesh serialization format version.
///
constexpr uint32_t mesh_format_version()
{
    return 1;
}

///
/// Serialize a SurfaceMesh to a byte buffer.
///
/// @param[in]  mesh     The mesh to serialize.
/// @param[in]  options  Serialization options (compression, etc.).
///
/// @tparam     Scalar   Mesh scalar type.
/// @tparam     Index    Mesh index type.
///
/// @return     A byte buffer containing the serialized mesh.
///
template <typename Scalar, typename Index>
LA_SERIALIZATION2_API std::vector<uint8_t> serialize_mesh(
    const SurfaceMesh<Scalar, Index>& mesh,
    const SerializeOptions& options = {});

///
/// Deserialize a SurfaceMesh from a byte buffer.
///
/// The function auto-detects whether the buffer is compressed. If the buffer contains a SimpleScene
/// or Scene instead of a SurfaceMesh, it can be automatically converted to a mesh when
/// DeserializeOptions::allow_scene_conversion is enabled. If the buffer was serialized with
/// different Scalar/Index types, type casting can be enabled via
/// DeserializeOptions::allow_type_cast.
///
/// @param[in]  buffer    A byte buffer containing the serialized data.
/// @param[in]  options   Deserialization options.
///
/// @tparam     MeshType  Mesh type (e.g. SurfaceMesh<float, uint32_t>).
///
/// @return     The deserialized mesh.
///
template <typename MeshType>
LA_SERIALIZATION2_API MeshType
deserialize_mesh(span<const uint8_t> buffer, const DeserializeOptions& options = {});

///
/// Save a SurfaceMesh to a file.
///
/// The suggested file extension is `.lgm`.
///
/// @param[in]  filename  Output file path.
/// @param[in]  mesh      The mesh to save.
/// @param[in]  options   Serialization options (compression, etc.).
///
/// @tparam     Scalar    Mesh scalar type.
/// @tparam     Index     Mesh index type.
///
template <typename Scalar, typename Index>
LA_SERIALIZATION2_API void save_mesh(
    const fs::path& filename,
    const SurfaceMesh<Scalar, Index>& mesh,
    const SerializeOptions& options = {});

///
/// Load a SurfaceMesh from a file.
///
/// Reads the file into memory and calls deserialize_mesh(). The function auto-detects whether the
/// file contents are compressed.
///
/// @param[in]  filename  Input file path.
/// @param[in]  options   Deserialization options.
///
/// @tparam     MeshType  Mesh type (e.g. SurfaceMesh<float, uint32_t>).
///
/// @return     The loaded mesh.
///
/// @see        deserialize_mesh() for details on scene conversion and type casting options.
///
template <typename MeshType>
LA_SERIALIZATION2_API MeshType
load_mesh(const fs::path& filename, const DeserializeOptions& options = {});

/// @}

} // namespace lagrange::serialization

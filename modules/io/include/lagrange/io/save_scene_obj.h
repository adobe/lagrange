/*
 * Copyright 2025 Adobe. All rights reserved.
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
#include <lagrange/io/types.h>
#include <lagrange/scene/Scene.h>

#include <iosfwd>


namespace lagrange::io {

/**
 * Save a scene to an obj file. The following information will be saved:
 * - Geometry (vertices, faces)
 * - UV coordinates (if present)
 * - Normals (if present)
 * - Material assignments (if present)
 * - Object and group names
 *
 * Notes:
 * - Some scene features such as hierarchical transforms, cameras, lights, and custom attributes
 * are not supported by the OBJ format and will be lost.
 * - Stream-based save_scene_obj does not support writing an .mtl file or texture images. Consider
 * using file-based save_scene_obj instead.
 *
 * @param output_stream Stream to output data
 * @param scene         Scene to save
 * @param options       SaveOptions, check the struct for more details.
 */
template <typename Scalar, typename Index>
void save_scene_obj(
    std::ostream& output_stream,
    const scene::Scene<Scalar, Index>& scene,
    const SaveOptions& options = {});

/**
 * Save a scene to an obj file. The following information will be saved:
 * - Geometry (vertices, faces)
 * - UV coordinates (if present)
 * - Normals (if present)
 * - Material assignments (if present)
 * - Object and group names
 * - Materials will be saved to a separate .mtl file, but will be (poorly) converted from PBR to
 * phong.
 * - Base color and normal textures (if present) will be saved to separate files in the same
 * directory.
 *
 * Notes:
 * - Some scene features such as hierarchical transforms, cameras, lights, and custom attributes
 * are not supported by the OBJ format and will be lost.
 *
 * @param filename      path to output file
 * @param scene         Scene to save
 * @param options       SaveOptions, check the struct for more details.
 */
template <typename Scalar, typename Index>
void save_scene_obj(
    const fs::path& filename,
    const scene::Scene<Scalar, Index>& scene,
    const SaveOptions& options = {});

} // namespace lagrange::io

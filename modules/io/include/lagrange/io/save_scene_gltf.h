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
#pragma once

#include <lagrange/fs/filesystem.h>
#include <lagrange/io/types.h>
#include <lagrange/scene/Scene.h>

#include <iosfwd>

namespace lagrange::io {

/**
 * Save a scene to a gltf or glb file.
 *
 * @param output_stream Stream to output data
 * @param scene         Scene to save
 * @param options       SaveOptions, check the struct for more details.
 */
template <typename Scalar, typename Index>
void save_scene_gltf(
    std::ostream& output_stream,
    const scene::Scene<Scalar, Index>& scene,
    const SaveOptions& options = {});

/**
 * Save a scene to a gltf or glb file.
 *
 * @param filename      path to output file
 * @param scene         Scene to save
 * @param options       SaveOptions, check the struct for more details.
 */
template <typename Scalar, typename Index>
void save_scene_gltf(
    const fs::path& filename,
    const scene::Scene<Scalar, Index>& scene,
    const SaveOptions& options = {});

}

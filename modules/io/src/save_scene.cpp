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

#include <lagrange/io/save_scene.h>

#include <lagrange/io/api.h>
#include <lagrange/io/save_scene_gltf.h>
#include <lagrange/scene/SceneTypes.h>
#include <lagrange/utils/strings.h>

#include <ostream>

namespace lagrange::io {

template <typename Scalar, typename Index>
void save_scene(
    const fs::path& filename,
    const scene::Scene<Scalar, Index>& scene,
    const SaveOptions& options)
{
    std::string ext = to_lower(filename.extension().string());

    if (ext == ".gltf" || ext == ".glb") {
        save_scene_gltf(filename, scene, options);
    } else {
        // todo
        throw std::runtime_error("Unsupported format or not implemented yet!");
    }
}

template <typename Scalar, typename Index>
void save_scene(
    std::ostream& output_stream,
    const scene::Scene<Scalar, Index>& scene,
    FileFormat format,
    const SaveOptions& options)
{
    switch (format) {
    case FileFormat::Gltf: save_scene_gltf(output_stream, scene, options); break;
    default: throw std::runtime_error("Unrecognized file format!");
    }
}

#define LA_X_save_scene(_, S, I)         \
    template LA_IO_API void save_scene(  \
        const fs::path& filename,        \
        const scene::Scene<S, I>& scene, \
        const SaveOptions& options);     \
    template LA_IO_API void save_scene(  \
        std::ostream& output_stream,     \
        const scene::Scene<S, I>& scene, \
        FileFormat format,               \
        const SaveOptions& options);
LA_SCENE_X(save_scene, 0);
#undef LA_X_save_scene

} // namespace lagrange::io

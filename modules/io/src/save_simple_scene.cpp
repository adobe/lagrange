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


#include <lagrange/io/save_simple_scene.h>

#include <lagrange/Logger.h>
#include <lagrange/scene/SimpleSceneTypes.h>

#include <lagrange/io/api.h>
#include <lagrange/io/save_mesh_msh.h>
#include <lagrange/io/save_mesh_obj.h>
#include <lagrange/io/save_mesh_ply.h>
#include <lagrange/io/save_simple_scene_gltf.h>
#include <lagrange/utils/strings.h>

namespace lagrange::io {

template <typename Scalar, typename Index, size_t Dimension>
void save_simple_scene(
    const fs::path& filename,
    const scene::SimpleScene<Scalar, Index, Dimension>& scene,
    const SaveOptions& options)
{
    std::string ext = to_lower(filename.extension().string());
    if (ext == ".obj") {
        // todo
        throw std::runtime_error("Not implemented yet!");
    } else if (ext == ".ply") {
        // todo
        throw std::runtime_error("Not implemented yet!");
    } else if (ext == ".msh") {
        // todo
        throw std::runtime_error("Not implemented yet!");
    } else if (ext == ".gltf" || ext == ".glb") {
        save_simple_scene_gltf(filename, scene, options);
    }
}

#define LA_X_save_simple_scene(_, S, I, D)        \
    template LA_IO_API void save_simple_scene(              \
        const fs::path& filename,                 \
        const scene::SimpleScene<S, I, D>& scene, \
        const SaveOptions& options);
LA_SIMPLE_SCENE_X(save_simple_scene, 0);
#undef LA_X_save_simple_scene

} // namespace lagrange::io

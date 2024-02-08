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

#include <lagrange/Logger.h>
#include <lagrange/io/load_simple_scene.h>
#include <lagrange/io/load_simple_scene_fbx.h>
#include <lagrange/io/load_simple_scene_gltf.h>
#include <lagrange/scene/SimpleScene.h>
#include <lagrange/scene/SimpleSceneTypes.h>
#include <lagrange/utils/strings.h>

#ifdef LAGRANGE_WITH_ASSIMP
    #include <lagrange/io/load_simple_scene_assimp.h>
#endif

namespace lagrange::io {

template <typename SceneType>
SceneType load_simple_scene(const fs::path& filename, const LoadOptions& options)
{
    std::string ext = to_lower(filename.extension().string());
    if (ext == ".gltf" || ext == ".glb") {
        return load_simple_scene_gltf<SceneType>(filename, options);
    } else if (ext == ".fbx") {
        return load_simple_scene_fbx<SceneType>(filename, options);
    } else {
#ifdef LAGRANGE_WITH_ASSIMP
        return load_simple_scene_assimp<SceneType>(filename, options);
#else
        logger().error("Unsupported format. You may want to compile with LAGRANGE_WITH_ASSIMP=ON.");
#endif
    }
    return SceneType();
}
#define LA_X_load_simple_scene(_, S, I, D)                  \
    template scene::SimpleScene<S, I, D> load_simple_scene( \
        const fs::path& filename,                           \
        const LoadOptions& options);
LA_SIMPLE_SCENE_X(load_simple_scene, 0);

} // namespace lagrange::io

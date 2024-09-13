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

#include <lagrange/io/load_scene.h>

#include <lagrange/Logger.h>
#include <lagrange/io/api.h>
#include <lagrange/io/internal/detect_file_format.h>
#include <lagrange/io/load_scene_fbx.h>
#include <lagrange/io/load_scene_gltf.h>
#include <lagrange/io/load_scene_obj.h>
#include <lagrange/scene/SceneTypes.h>
#include <lagrange/utils/strings.h>

#ifdef LAGRANGE_WITH_ASSIMP
    #include <lagrange/io/load_scene_assimp.h>
#endif

#include <istream>

namespace lagrange::io {

template <typename SceneType>
SceneType load_scene(const fs::path& filename, const LoadOptions& options)
{
    std::string ext = to_lower(filename.extension().string());
    if (ext == ".gltf" || ext == ".glb") {
        return load_scene_gltf<SceneType>(filename, options);
    } else if (ext == ".fbx") {
        return load_scene_fbx<SceneType>(filename, options);
    } else if (ext == ".obj") {
        return load_scene_obj<SceneType>(filename, options);
    } else {
#ifdef LAGRANGE_WITH_ASSIMP
        return load_scene_assimp<SceneType>(filename, options);
#else
        logger().error("Unsupported format. You may want to compile with LAGRANGE_WITH_ASSIMP=ON.");
        return SceneType();
#endif
    }
}

template <typename SceneType>
SceneType load_scene(std::istream& input_stream, const LoadOptions& options)
{
    switch (internal::detect_file_format(input_stream)) {
    case FileFormat::Gltf: return load_scene_gltf<SceneType>(input_stream, options);
    case FileFormat::Fbx: return load_scene_fbx<SceneType>(input_stream, options);
    default: throw std::runtime_error("Unsupported format.");
    }
}


#define LA_X_load_scene(_, S, I)                      \
    template LA_IO_API scene::Scene<S, I> load_scene( \
        const fs::path& filename,                     \
        const LoadOptions& options);                  \
    template LA_IO_API scene::Scene<S, I> load_scene( \
        std::istream& input_stream,                   \
        const LoadOptions& options);

LA_SCENE_X(load_scene, 0);

} // namespace lagrange::io

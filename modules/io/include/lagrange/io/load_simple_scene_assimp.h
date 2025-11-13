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

#ifdef LAGRANGE_WITH_ASSIMP

    #include <lagrange/fs/filesystem.h>
    #include <lagrange/io/types.h>
    #include <lagrange/scene/SimpleScene.h>

    #include <iosfwd>

namespace lagrange::io {

/**
 * Load a simple scene using assimp.
 *
 * @param[in] filename input file name
 * @param[in] options
 *
 * @return loaded scene
 */
template <typename SceneType>
SceneType load_simple_scene_assimp(const fs::path& filename, const LoadOptions& options = {});

/**
 * Load a simple scene using assimp.
 *
 * @param[in] input_stream  Input stream containing the scene data
 * @param[in] options
 *
 * @return loaded scene
 */
template <typename SceneType>
SceneType load_simple_scene_assimp(std::istream& input_stream, const LoadOptions& options = {});

} // namespace lagrange::io

#endif

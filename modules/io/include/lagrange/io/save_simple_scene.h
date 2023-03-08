/*
 * Copyright 2022 Adobe. All rights reserved.
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
#include <lagrange/scene/SimpleScene.h>

namespace lagrange::io {

/**
 * Save a mesh to a file.
 *
 * @param[in] filename  path to output
 * @param[in] scene     mesh to save
 * @param[in] options   SaveOptions, check the struct for more details.
 */
template <typename Scalar, typename Index, size_t Dimension = 3>
void save_simple_scene(
    const fs::path& filename,
    const scene::SimpleScene<Scalar, Index, Dimension>& scene,
    const SaveOptions& options = {});

} // namespace lagrange::io

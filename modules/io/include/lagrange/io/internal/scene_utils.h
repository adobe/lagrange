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

#include <lagrange/io/types.h>
#include <lagrange/scene/Scene.h>

namespace lagrange::io::internal {

/**
 * Load an image from disk, and store it in the Image.
 *
 * @param[in] name.     Name of texture or relative or full path to texture.
 * @param[in] options.  Options. Remember to set options.search_path if necessary.
 * @param[out] image.   This will be filled with the loaded data.
 *
 * @return true if successful.
 */
bool try_load_image(
    const std::string& name,
    const LoadOptions& options,
    scene::ImageExperimental& image);

} // namespace lagrange::io::internal

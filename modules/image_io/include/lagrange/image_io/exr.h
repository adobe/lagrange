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
#include <lagrange/image/RawInputImage.h>

namespace lagrange {
namespace image_io {

enum class TinyexrPixelType : unsigned int { uint32, float32, unknown };

bool load_image_exr(
    const lagrange::fs::path& path,
    void** out_rgba,
    int* width,
    int* height,
    int* components,
    TinyexrPixelType* pixeltype);

bool save_image_exr(
    const lagrange::fs::path& path,
    const void* data,
    const int width,
    const int height,
    const int components,
    const TinyexrPixelType pixeltype);

} // namespace image_io
} // namespace lagrange

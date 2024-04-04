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
#include <lagrange/image/ImageView.h>
#include <lagrange/image_io/api.h>

namespace lagrange {
namespace image_io {

// Save image. Returns true on success.
LA_IMAGE_IO_API bool save_image(
    const fs::path& path,
    const unsigned char* data,
    size_t width,
    size_t height,
    image::ImagePrecision precision,
    image::ImageChannel channel);

// Save image using stb. Supports png or jpeg. Only supports uint8 data.
LA_IMAGE_IO_API bool save_image_stb(
    const fs::path& path,
    const unsigned char* data,
    size_t width,
    size_t height,
    image::ImageChannel channel);

// Note: #include <lagrange/image_io/exr.h> to use the full save_image_exr directly
// Save exr image using tinyexr.
LA_IMAGE_IO_API bool save_image_exr(
    const fs::path& path,
    const unsigned char* data,
    size_t width,
    size_t height,
    image::ImagePrecision precision,
    image::ImageChannel channel);

// Save image to our custom binary format.
LA_IMAGE_IO_API bool save_image_bin(
    const fs::path& path,
    const unsigned char* data,
    size_t width,
    size_t height,
    image::ImagePrecision precision,
    image::ImageChannel channel);

// Save image from an ImageView.
template <typename T>
bool save_image(const fs::path& path, const image::ImageView<T>& img)
{
    auto buf = std::move(img.pack());
    return save_image(
        path,
        buf.data(),
        img.get_view_size()(0),
        img.get_view_size()(1),
        image::ImageTraits<T>::precision,
        image::ImageTraits<T>::channel);
}

} // namespace image_io
} // namespace lagrange

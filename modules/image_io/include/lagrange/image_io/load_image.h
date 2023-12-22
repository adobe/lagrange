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

namespace lagrange {
namespace image_io {

struct LoadImageResult
{
    bool valid = false;
    size_t width = 0;
    size_t height = 0;
    image::ImagePrecision precision = image::ImagePrecision::unknown;
    image::ImageChannel channel = image::ImageChannel::unknown;
    std::shared_ptr<image::ImageStorage> storage = nullptr;
};

// Load image. Storage type is determined by the image file type.
LoadImageResult load_image(const fs::path& path);

// Load png or jpg image using stb library. Produces uint8 data.
LoadImageResult load_image_stb(const fs::path& path);

// Note: #include <lagrange/image_io/exr.h> to use the full load_image_exr directly
// Load exr image using tinyexr. Produces multiple data types.
LoadImageResult load_image_exr(const fs::path& path);

// Load image from our custom binary format.
LoadImageResult load_image_bin(const fs::path& path);

// Load image as the provided type/view. Converts if needed/possible. Returns true on success.
template <typename T>
bool load_image_as(const fs::path& path, image::ImageView<T>& img)
{
    LoadImageResult result = load_image(path);
    if (!result.valid) return false;

    if (image::ImageTraits<T>::precision == result.precision &&
        image::ImageTraits<T>::channel == result.channel) {
        return img.view(
            result.storage,
            result.storage->get_full_size()(0) / sizeof(T),
            result.storage->get_full_size()(1),
            sizeof(T),
            1,
            0,
            0);
    }

    bool valid_conversion = false;
#define LAGRANGE_TMP(PRECISION, CHANNEL, TYPE)                   \
    if (image::ImagePrecision::PRECISION == result.precision &&  \
        image::ImageChannel::CHANNEL == result.channel) {        \
        image::ImageView<TYPE> temp_image_view(                  \
            result.storage,                                      \
            result.storage->get_full_size()(0) / sizeof(TYPE),   \
            result.storage->get_full_size()(1),                  \
            sizeof(TYPE),                                        \
            1,                                                   \
            0,                                                   \
            0);                                                  \
        valid_conversion = img.convert_from(temp_image_view, 1); \
    }
#define LAGRANGE_TMP_COMMA ,
    LAGRANGE_TMP(uint8, one, unsigned char)
    else LAGRANGE_TMP(uint8, three, Eigen::Matrix<unsigned char LAGRANGE_TMP_COMMA 3 LAGRANGE_TMP_COMMA 1>) else LAGRANGE_TMP(
        uint8,
        four,
        Eigen::
            Matrix<
                unsigned char LAGRANGE_TMP_COMMA 4 LAGRANGE_TMP_COMMA 1>) else LAGRANGE_TMP(float32, one, float) else LAGRANGE_TMP(float32, three, Eigen::Vector3f) else LAGRANGE_TMP(float32, four, Eigen::Vector4f) else LAGRANGE_TMP(float64, one, double) else LAGRANGE_TMP(float64, three, Eigen::Vector3d) else LAGRANGE_TMP(float64, four, Eigen::Vector4d) return valid_conversion;
#undef LAGRANGE_TMP
#undef LAGRANGE_TMP_COMMA
}

} // namespace image_io
} // namespace lagrange

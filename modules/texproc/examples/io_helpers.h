/*
 * Copyright 2025 Adobe. All rights reserved.
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

#include <lagrange/Logger.h>
#include <lagrange/image/Array3D.h>
#include <lagrange/image/View3D.h>
#include <lagrange/image_io/exr.h>
#include <lagrange/image_io/load_image.h>
#include <lagrange/scene/Scene.h>
#include <lagrange/utils/assert.h>

#include <algorithm>

using Array3Df = lagrange::image::experimental::Array3D<float>;
using View3Df = lagrange::image::experimental::View3D<float>;

template <size_t NumChannels, typename Precision>
Array3Df convert_from(const lagrange::image_io::LoadImageResult& img)
{
    size_t width = img.width;
    size_t height = img.height;
    Array3Df result =
        lagrange::image::experimental::create_image<float>(width, height, NumChannels);

    using SourcePixel =
        std::conditional_t<NumChannels == 1, Precision, Eigen::Vector<Precision, NumChannels>>;
    using TargetPixel =
        std::conditional_t<NumChannels == 1, float, Eigen::Vector<float, NumChannels>>;

    lagrange::image::ImageView<SourcePixel> source_view(
        img.storage,
        img.storage->get_full_size()(0) / sizeof(SourcePixel),
        img.storage->get_full_size()(1),
        sizeof(SourcePixel),
        1,
        0,
        0);

    lagrange::image::ImageView<TargetPixel> target_view(width, height, 1);
    la_runtime_assert(target_view.convert_from(source_view, 1));

    for (size_t x = 0; x < width; ++x) {
        for (size_t y = 0; y < height; ++y) {
            auto pixel = target_view(x, y);
            if constexpr (NumChannels == 1) {
                result(x, y, 0) = pixel;
            } else {
                for (size_t c = 0; c < NumChannels; ++c) {
                    result(x, y, c) = pixel[c];
                }
            }
        }
    }

    return result;
}

template <size_t NumChannels>
Array3Df convert_from(const lagrange::image_io::LoadImageResult& img)
{
    switch (img.precision) {
    case lagrange::image::ImagePrecision::float32: return convert_from<NumChannels, float>(img);
    case lagrange::image::ImagePrecision::float64: return convert_from<NumChannels, double>(img);
    case lagrange::image::ImagePrecision::uint8:
        return convert_from<NumChannels, unsigned char>(img);
    case lagrange::image::ImagePrecision::int8: return convert_from<NumChannels, char>(img);
    case lagrange::image::ImagePrecision::uint32: return convert_from<NumChannels, uint32_t>(img);
    case lagrange::image::ImagePrecision::int32: return convert_from<NumChannels, int32_t>(img);
    case lagrange::image::ImagePrecision::float16:
        return convert_from<NumChannels, Eigen::half>(img);
    default: throw std::runtime_error("Unsupported precision");
    }
}

inline Array3Df load_image(const lagrange::fs::path& path)
{
    auto img = lagrange::image_io::load_image(path);

    switch (img.channel) {
    case lagrange::image::ImageChannel::one: return convert_from<1>(img);
    case lagrange::image::ImageChannel::three: return convert_from<3>(img);
    case lagrange::image::ImageChannel::four: return convert_from<4>(img);
    default: throw std::runtime_error("Unsupported number of channels");
    }
}

inline void save_image(lagrange::fs::path path, View3Df image)
{
    if (path.extension() != ".exr") {
        lagrange::logger().warn("Only .exr output files are supported. Saving as .exr.");
        path = path.replace_extension(".exr");
    }

    using namespace lagrange::image;

    const size_t width = image.extent(0);
    const size_t height = image.extent(1);
    const size_t num_channels = image.extent(2);

    std::vector<float> scanline(width * height * num_channels);
    for (size_t x = 0; x < width; ++x) {
        for (size_t y = 0; y < height; ++y) {
            for (size_t c = 0; c < num_channels; ++c) {
                scanline[y * width * num_channels + x * num_channels + c] = image(x, y, c);
            }
        }
    }

    lagrange::image_io::save_image_exr(
        path,
        static_cast<const void*>(scanline.data()),
        static_cast<int>(width),
        static_cast<int>(height),
        static_cast<int>(num_channels),
        lagrange::image_io::TinyexrPixelType::float32);
}

inline void sort_paths(std::vector<lagrange::fs::path>& paths)
{
    auto original = paths;
    std::sort(paths.begin(), paths.end());
    if (!std::is_sorted(original.begin(), original.end())) {
        lagrange::logger().warn("Input filenames were not sorted. Using sorted order.");
    }
}

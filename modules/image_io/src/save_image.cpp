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
#include <lagrange/image_io/save_image.h>

#include <lagrange/Logger.h>
#include <lagrange/image_io/common.h>
#include <lagrange/image_io/exr.h>

#include <stb_image_write.h>

#include <fstream>

namespace lagrange {
namespace image_io {

bool save_image(
    const fs::path& input_path,
    const unsigned char* data,
    const size_t width,
    const size_t height,
    const image::ImagePrecision precision,
    const image::ImageChannel channel)
{
    // basic sanity check
    if (input_path.empty() || !data || !width || !height ||
        image::ImagePrecision::unknown == precision || image::ImageChannel::unknown == channel) {
        logger().error(
            "save_image error: invalid input (fn, data, width, height, precision, "
            "channel): {}, {}, {}, {}, {}, {}",
            input_path.string(),
            fmt::ptr(data),
            width,
            height,
            static_cast<unsigned int>(precision),
            static_cast<unsigned int>(channel));
        return false;
    }
    const auto file_type = precision_to_file_type(precision);
    if (FileType::unknown == file_type) {
        logger().error(
            "save_image error, unknown file_type '{}'",
            static_cast<unsigned int>(file_type));
        return false;
    }

    // get or add file extension
    const auto file_extension = file_type_to_file_extension(file_type);

    auto path = input_path;
    auto ext = input_path.extension().string();
    if (ext.empty()) {
        ext = file_extension;
        path += ext;
    }
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (file_extension != ext) {
        logger().error(
            "save_image error: invalid extension '{}', should be '{}'. {}.",
            ext,
            file_extension,
            path.string());
        return false;
    }

    // check for image precision and file extension
    if (FileType::png == file_type || FileType::jpg == file_type) {
        if (image::ImagePrecision::uint8 != precision) {
            logger().error("save_image error: *.png only supports uint8. {}", path.string());
            return false;
        } else {
            return stbi_write_png(
                path.string().c_str(),
                static_cast<int>(width),
                static_cast<int>(height),
                static_cast<int>(channel),
                data,
                0);
        }

    } else if (FileType::exr == file_type) {
        return save_image_exr(path, data, width, height, precision, channel);

    } else if (FileType::bin == file_type) {
        return save_image_bin(path, data, width, height, precision, channel);

    } else {
        logger().error("save_image error: unknown file_type. {}", path.string());
        return false;
    }

    return true;
};

bool save_image_stb(
    const fs::path& path,
    const unsigned char* data,
    size_t width,
    size_t height,
    image::ImageChannel channel)
{
    return stbi_write_png(
        path.string().c_str(),
        static_cast<int>(width),
        static_cast<int>(height),
        static_cast<int>(channel),
        data,
        0);
}

bool save_image_exr(
    const fs::path& path,
    const unsigned char* data,
    size_t width,
    size_t height,
    image::ImagePrecision precision,
    image::ImageChannel channel)
{
    if (image::ImagePrecision::uint32 != precision && image::ImagePrecision::float32 != precision) {
        logger().error("save_image error: *.exr only supports uint32 and float32. {}", path.string());
        return false;
    }
    if (image::ImagePrecision::uint32 == precision) {
        return save_image_exr(
            path,
            reinterpret_cast<const void*>(data),
            static_cast<int>(width),
            static_cast<int>(height),
            static_cast<int>(channel),
            TinyexrPixelType::uint32);
    }
    if (image::ImagePrecision::float32 == precision) {
        return save_image_exr(
            path,
            reinterpret_cast<const void*>(data),
            static_cast<int>(width),
            static_cast<int>(height),
            static_cast<int>(channel),
            TinyexrPixelType::float32);
    }
    return false;
}

bool save_image_bin(
    const fs::path& path,
    const unsigned char* data,
    size_t width,
    size_t height,
    image::ImagePrecision precision,
    image::ImageChannel channel)
{
    const auto header = precision_to_bin_header(precision);
    if ("" == header) {
        logger().error("save_image error: cannot get valid header for *.bin. {}", path.string());
        return false;
    }
    std::ofstream ofs(path, std::ios_base::binary);
    if (!ofs.is_open()) {
        logger().error("save_image error: cannot open file. {}", path.string());
        return false;
    }

    ofs << header << " " << width << " " << height << " " << static_cast<unsigned int>(channel)
        << "\n";
    ofs.write(
        reinterpret_cast<const char*>(data),
        size_of_precision(precision) * static_cast<size_t>(channel) * width * height);
    ofs.close();
    return true;
}

} // namespace image_io
} // namespace lagrange

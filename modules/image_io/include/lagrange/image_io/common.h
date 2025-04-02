/*
 * Copyright 2020 Adobe. All rights reserved.
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

#include <lagrange/image/ImageType.h>
#include <string>

namespace lagrange {
namespace image_io {

enum class FileType { png, jpg, exr, bin, unknown };

inline FileType file_extension_to_file_type(const std::string& extension)
{
    if (".png" == extension) {
        return FileType::png;
    } else if (".jpg" == extension || ".jpeg" == extension) {
        return FileType::jpg;
    } else if (".exr" == extension) {
        return FileType::exr;
    } else if (".bin" == extension) {
        return FileType::bin;
    } else {
        return FileType::unknown;
    }
}
inline std::string file_type_to_file_extension(FileType type)
{
    switch (type) {
    case FileType::png: return ".png";
    case FileType::jpg: return ".jpg";
    case FileType::exr: return ".exr";
    case FileType::bin: return ".bin";
    case FileType::unknown: return "";
    default: return "";
    }
}

inline FileType precision_to_file_type(image::ImagePrecision precision)
{
    switch (precision) {
    case lagrange::image::ImagePrecision::uint8: return FileType::png;
    case lagrange::image::ImagePrecision::uint32:
    case lagrange::image::ImagePrecision::float32: return FileType::exr;
    case lagrange::image::ImagePrecision::int8:
    case lagrange::image::ImagePrecision::int32:
    case lagrange::image::ImagePrecision::float64: return FileType::bin;
    case lagrange::image::ImagePrecision::unknown:
    default: return FileType::unknown;
    }
}

inline size_t size_of_precision(image::ImagePrecision precision)
{
    switch (precision) {
    case lagrange::image::ImagePrecision::uint8:
    case lagrange::image::ImagePrecision::int8: return 1;
    case lagrange::image::ImagePrecision::uint32:
    case lagrange::image::ImagePrecision::int32:
    case lagrange::image::ImagePrecision::float32: return 4;
    case lagrange::image::ImagePrecision::float64: return 8;
    case lagrange::image::ImagePrecision::unknown:
    default: return 0;
    }
}

inline std::string precision_to_bin_header(image::ImagePrecision precision)
{
    if (image::ImagePrecision::int8 == precision) {
        return "int8";
    } else if (image::ImagePrecision::int32 == precision) {
        return "int32";
    } else if (image::ImagePrecision::float64 == precision) {
        return "float64";
    } else {
        return "";
    }
}
inline image::ImagePrecision bin_header_to_precision(const std::string& header)
{
    if ("int8" == header) {
        return image::ImagePrecision::int8;
    } else if ("int32" == header) {
        return image::ImagePrecision::int32;
    } else if ("float64" == header) {
        return image::ImagePrecision::float64;
    } else {
        return image::ImagePrecision::unknown;
    }
}

} // namespace image_io
} // namespace lagrange

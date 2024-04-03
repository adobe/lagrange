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

#include <lagrange/AttributeValueType.h>
#include <lagrange/image_io/load_image.h>
#include <lagrange/io/internal/scene_utils.h>

namespace lagrange::io::internal {

bool try_load_image(
    const std::string& name,
    const LoadOptions& options,
    scene::ImageExperimental& image)
{
    fs::path path = name;
    if (path.is_relative() && !options.search_path.empty()) path = options.search_path / name;
    if (path.empty()) return false;

    image_io::LoadImageResult result = image_io::load_image(path);
    if (!result.valid) return false;

    scene::ImageBufferExperimental& buffer = image.image;
    buffer.width = result.width;
    buffer.height = result.height;
    buffer.num_channels = static_cast<size_t>(result.channel);
    switch (result.precision) {
    case image::ImagePrecision::uint8: buffer.element_type = AttributeValueType::e_uint8_t; break;
    case image::ImagePrecision::int8: buffer.element_type = AttributeValueType::e_int8_t; break;
    case image::ImagePrecision::uint32: buffer.element_type = AttributeValueType::e_uint32_t; break;
    case image::ImagePrecision::int32: buffer.element_type = AttributeValueType::e_int32_t; break;
    case image::ImagePrecision::float32: buffer.element_type = AttributeValueType::e_float; break;
    case image::ImagePrecision::float64: buffer.element_type = AttributeValueType::e_double; break;
    case image::ImagePrecision::float16: [[fallthrough]];
    default: throw std::runtime_error("Unsupported image precision");
    }

    la_runtime_assert(result.storage != nullptr);
    const size_t num_bytes =
        buffer.width * buffer.height * buffer.num_channels * buffer.get_bits_per_element() / 8;
    buffer.data.reserve(num_bytes);
    std::copy(
        result.storage->data(),
        result.storage->data() + num_bytes,
        std::back_inserter(buffer.data));

    return true;
}

} // namespace lagrange::io::internal

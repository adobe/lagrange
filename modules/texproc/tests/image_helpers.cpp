/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include "image_helpers.h"

#include <lagrange/AttributeValueType.h>

auto test::scene_image_to_image_array(const lagrange::scene::ImageBufferExperimental& image)
    -> lagrange::image::experimental::Array3D<float>
{
    auto result = lagrange::image::experimental::create_image<float>(
        image.width,
        image.height,
        image.num_channels);

    auto copy_buffer = [&](auto scalar) {
        using T = std::decay_t<decltype(scalar)>;
        constexpr bool IsChar = std::is_integral_v<T> && sizeof(T) == 1;
        la_runtime_assert(sizeof(T) * 8 == image.get_bits_per_element());
        auto rawbuf = reinterpret_cast<const T*>(image.data.data());
        for (size_t y = 0, i = 0; y < image.height; ++y) {
            for (size_t x = 0; x < image.width; ++x) {
                for (size_t c = 0; c < image.num_channels; ++c) {
                    if constexpr (IsChar) {
                        result(x, y, c) = static_cast<float>(rawbuf[i++]) / 255.f;
                    } else {
                        result(x, y, c) = rawbuf[i++];
                    }
                }
            }
        }
    };

    using lagrange::AttributeValueType;
    switch (image.element_type) {
    case AttributeValueType::e_uint8_t: copy_buffer(uint8_t()); break;
    case AttributeValueType::e_int8_t: copy_buffer(int8_t()); break;
    case AttributeValueType::e_uint32_t: copy_buffer(uint32_t()); break;
    case AttributeValueType::e_int32_t: copy_buffer(int32_t()); break;
    case AttributeValueType::e_float: copy_buffer(float()); break;
    case AttributeValueType::e_double: copy_buffer(double()); break;
    default: throw std::runtime_error("Unsupported image scalar type");
    }

    return result;
}

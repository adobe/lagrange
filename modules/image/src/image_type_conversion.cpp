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
#include <lagrange/image/image_type_conversion.h>
#include <lagrange/utils/assert.h>

namespace lagrange {
namespace image {

std::shared_ptr<ImageStorage> image_storage_from_raw_input_image(const image::RawInputImage& image)
{
    const size_t size_pixel = image.get_size_pixel();
    const size_t stride = image.get_row_stride();
    const size_t pixel_data_offset = image.get_pixel_data_offset();
    const unsigned char* data =
        static_cast<const unsigned char*>(image.get_pixel_data()) - pixel_data_offset;
    return std::make_shared<ImageStorage>(
        size_pixel * image.get_width(),
        image.get_height(),
        stride,
        const_cast<unsigned char*>(data));
}

image::RawInputImage raw_input_image_from_image_view(image::ImageViewBase& in, bool copy_buffer)
{
    image::RawInputImage out;

    out.set_width(in.get_view_size()(0));
    out.set_height(in.get_view_size()(1));

    out.set_row_byte_stride(in.get_view_stride_in_byte()(1));

    const auto channel = in.get_channel();
    if (channel == image::ImageChannel::one) {
        out.set_tex_format(lagrange::image::RawInputImage::texture_format::luminance);
    } else if (channel == image::ImageChannel::three) {
        out.set_tex_format(lagrange::image::RawInputImage::texture_format::rgb);
    } else if (channel == image::ImageChannel::four) {
        out.set_tex_format(lagrange::image::RawInputImage::texture_format::rgba);
    } else {
        throw std::runtime_error("failed in raw_input_image_from_image_view: unsupported channel!");
    }

    const auto precision = in.get_precision();
    if (precision == image::ImagePrecision::uint8) {
        out.set_pixel_precision(lagrange::image::RawInputImage::precision_semantic::byte_p);
    } else if (precision == image::ImagePrecision::float32) {
        out.set_pixel_precision(lagrange::image::RawInputImage::precision_semantic::single_p);
    } else if (precision == image::ImagePrecision::float64) {
        out.set_pixel_precision(lagrange::image::RawInputImage::precision_semantic::double_p);
    } else {
        throw std::runtime_error(
            "failed in raw_input_image_from_image_view: unsupported precision!");
    }

    out.set_color_space(lagrange::image::RawInputImage::color_space::linear); // fix?

    out.set_storage_format(
        lagrange::image::RawInputImage::image_storage_format::first_pixel_row_at_top); // fix?

    la_runtime_assert(in.is_compact());
    out.set_pixel_data(in.get_data(), copy_buffer);

    return out;
}

} // namespace image
} // namespace lagrange

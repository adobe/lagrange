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
#include <lagrange/image/RawInputImage.h>
#include <lagrange/utils/assert.h>

namespace lagrange {
namespace image {

// impl RawInputImage

void RawInputImage::set_pixel_data(const void* pixel_data, const bool copy_to_local)
{
    if (copy_to_local) {
        m_local_pixel_data.resize(get_row_stride() * m_height);
        std::copy_n(
            static_cast<const unsigned char*>(pixel_data) - get_pixel_data_offset(),
            m_local_pixel_data.size(),
            m_local_pixel_data.data());
        m_pixel_data = nullptr;
    } else {
        m_local_pixel_data.clear();
        m_pixel_data = pixel_data;
    }
}

void RawInputImage::set_pixel_data_buffer(std::vector<unsigned char> pixel_data_buffer)
{
    la_runtime_assert(pixel_data_buffer.size() >= get_row_stride() * m_height);
    m_local_pixel_data = std::move(pixel_data_buffer);
    m_pixel_data = nullptr;
}

const void* RawInputImage::get_pixel_data() const
{
    if (m_local_pixel_data.empty()) {
        return m_pixel_data;
    } else {
        return reinterpret_cast<const void*>(m_local_pixel_data.data() + get_pixel_data_offset());
    }
}

bool RawInputImage::operator==(const RawInputImage& other) const
{
    if (m_width != other.m_width || m_height != other.m_height ||
        get_row_stride() != other.get_row_stride() ||
        m_pixel_precision != other.m_pixel_precision || m_color_space != other.m_color_space ||
        m_tex_format != other.m_tex_format || m_wrap_u != other.m_wrap_u ||
        m_wrap_v != other.m_wrap_v || m_storage_format != other.m_storage_format) {
        return false;
    }

    const unsigned char* self_mem =
        static_cast<const unsigned char*>(get_pixel_data()) - get_pixel_data_offset();
    const unsigned char* other_mem =
        static_cast<const unsigned char*>(other.get_pixel_data()) - other.get_pixel_data_offset();
    const size_t self_row_stride = get_row_stride();
    const size_t other_row_stride = other.get_row_stride();
    const size_t self_row_size = get_size_pixel() * m_width;

    for (size_t r = 0; r < m_height; ++r) {
        if (!std::equal(
                self_mem + r * self_row_stride,
                self_mem + r * self_row_stride + self_row_size,
                other_mem + r * other_row_stride)) {
            return false;
        }
    }

    return true;
}

// Wrap a float * data into a Linear 4-component image. Pixel memory ownership is not transferred.
RawInputImage
make_default_rgba_image(std::size_t i_width, std::size_t i_height, const void* i_pixels)
{
    la_runtime_assert(i_width > 0);
    la_runtime_assert(i_height > 0);
    la_runtime_assert(i_pixels != nullptr);

    RawInputImage result;

    result.set_width(i_width);
    result.set_height(i_height);
    result.set_tex_format(RawInputImage::texture_format::rgba);
    result.set_row_byte_stride(4 * sizeof(float) * i_width);
    result.set_pixel_data(i_pixels, false);
    return result;
}

// Wrap a float * data into a Linear 1-component image. Pixel memory ownership is not transferred.
RawInputImage
make_default_luminance_image(std::size_t i_width, std::size_t i_height, const void* i_pixels)
{
    la_runtime_assert(i_width > 0);
    la_runtime_assert(i_height > 0);
    la_runtime_assert(i_pixels != nullptr);

    RawInputImage result;

    result.set_width(i_width);
    result.set_height(i_height);
    result.set_tex_format(RawInputImage::texture_format::luminance);
    result.set_row_byte_stride(1 * sizeof(float) * i_width);
    result.set_pixel_data(i_pixels, false);
    return result;
}

} // namespace image
} // namespace lagrange

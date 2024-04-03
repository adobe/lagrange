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

#include <lagrange/common.h>
#include <lagrange/image/api.h>

#include <cassert>
#include <vector>

namespace lagrange {
namespace image {

/// RawInputImage holds the basic info and the raw pointer (without ownership) to the image pixels
/// * there are not paddings inner or inter pixels
/// * there may be paddings between rows
/// * m_row_byte_stride can be zero, when there are not paddings between rows,
///   the row stride can be calculated according to m_pixel_precision, m_tex_format and m_width
/// * the memory of pixels is always with the top-down layout
///   if m_storage_format is image_storage_format::first_pixel_row_at_top
///       get_pixel_data points to the beginning of the memory block
///   if m_storage_format is image_storage_format::first_pixel_row_at_bottom
///       get_pixel_data points to the beginning of the bottom row
/// * get_pixel_data can pointer to external memory without ownership, or inside m_local_pixel_data with ownership
///   if set_pixel_data(*, false), m_pixel_data points to external memory, m_local_pixel_data is empty
///   if set_pixel_data(*, true),  m_pixel_data is nullptr, m_local_pixel_data holds the pixels
class LA_IMAGE_API RawInputImage
{
public:
    enum class precision_semantic : uint32_t {
        byte_p, ///< 8-bit uint8_t
        half_p, ///< 16-bit floating point
        single_p, ///< 32-bit floating point
        double_p ///< 64-bit floating point
    };

    enum class color_space : uint32_t {
        linear, ///< pixel format is in linear space
        sRGB ///< pixel format is in sRGB space
    };

    enum class texture_format : uint32_t {
        luminance = ((1) << 8) | 0x0,
        luminance_alpha = ((2) << 8) | 0x1,
        alpha_luminance = ((2) << 8) | 0x2,
        rgb = ((3) << 8) | 0x4,
        bgr = ((3) << 8) | 0x8,
        rgba = ((4) << 8) | 0x10,
        argb = ((4) << 8) | 0x12, ///< suboptimal format
        bgra = ((4) << 8) | 0x14, ///< suboptimal format
        max = ((4) << 8) | ((1 << 8) - 1) /// used for query the max channels of all format
    };

    /// first_pixel_row_at_bottom: y is up (like OpenGL texcoords)
    /// first_pixel_row_at_top: y is down just like most image file formats
    enum class image_storage_format : uint32_t {
        first_pixel_row_at_bottom, ///< +y up
        first_pixel_row_at_top ///< +y down
    };

    enum class wrap_mode : uint32_t {
        repeat, ///< tiles the texture
        clamp, ///< clamp to the last pixel on the edge
        mirror ///< tiles the texture, mirrored when the integer coord is odd
    };

    enum class texture_filtering : uint32_t {
        nearest, ///< nearest neighbor interpolation
        bilinear ///< bilinear interpolation
    };

    template <typename Scalar, uint32_t NumChannels>
    struct PixelTraits;

public:
    size_t get_width() const { return m_width; }
    size_t get_height() const { return m_height; }
    size_t get_row_byte_stride() const { return m_row_byte_stride; }
    precision_semantic get_pixel_precision() const { return m_pixel_precision; }
    color_space get_color_space() const { return m_color_space; }
    texture_format get_tex_format() const { return m_tex_format; }
    wrap_mode get_wrap_u() const { return m_wrap_u; }
    wrap_mode get_wrap_v() const { return m_wrap_v; }
    image_storage_format get_storage_format() const { return m_storage_format; }

    size_t get_size_precision() const
    {
        return static_cast<size_t>(1u << static_cast<uint32_t>(m_pixel_precision));
    }

    size_t get_num_channels() const
    {
        return static_cast<size_t>(static_cast<uint32_t>(m_tex_format) >> 8u);
    }

    size_t get_size_pixel() const { return get_size_precision() * get_num_channels(); }

    size_t get_row_stride() const
    {
        return (0 == m_row_byte_stride ? get_size_pixel() * m_width : m_row_byte_stride);
    }

    size_t get_pixel_data_offset() const
    {
        return image_storage_format::first_pixel_row_at_top == m_storage_format
                   ? 0
                   : get_row_stride() * (m_height - 1);
    }

    const void* get_pixel_data() const;

    void set_width(size_t x) { m_width = x; }
    void set_height(size_t x) { m_height = x; }
    void set_row_byte_stride(size_t x) { m_row_byte_stride = x; }
    void set_pixel_precision(precision_semantic x) { m_pixel_precision = x; }
    void set_color_space(color_space x) { m_color_space = x; }
    void set_tex_format(texture_format x) { m_tex_format = x; }
    void set_wrap_u(wrap_mode x) { m_wrap_u = x; }
    void set_wrap_v(wrap_mode x) { m_wrap_v = x; }
    void set_storage_format(image_storage_format x) { m_storage_format = x; }

    /// set_pixel_data must be called all other member variables are well set
    /// if copy_to_local == false:
    ///   m_local_pixel_data empty
    ///   m_pixel_data points to the external pixel_data
    /// else:
    ///   external pixel_data are copied to m_local_pixel_data
    ///   m_pixel_data is nullptr
    void set_pixel_data(const void* pixel_data, const bool copy_to_local);

    /// m_local_pixel_data takes over pixel_data_buffer
    /// m_pixel_data is set to nullptr
    void set_pixel_data_buffer(std::vector<unsigned char> pixel_data_buffer);

    bool operator==(const RawInputImage& other) const;
    bool operator!=(const RawInputImage& other) const { return !(*this == other); }

    ///
    /// Sample an image at a given location.
    ///
    /// @param[in]  u               First sampling coordinate.
    /// @param[in]  v               Second sampling coordinate.
    /// @param[in]  filtering       Filtering scheme (nearest or bilinear).
    ///
    /// @tparam     TexcoordScalar  Texture coordinate type.
    /// @tparam     Scalar          Pixel scalar type.
    /// @tparam     NumChannels     Number of color channels in the pixel.
    ///
    /// @return     Pixel data.
    ///
    template <typename TexcoordScalar, typename Scalar, uint32_t NumChannels>
    typename PixelTraits<Scalar, NumChannels>::Pixel sample(
        const TexcoordScalar u,
        const TexcoordScalar v,
        const texture_filtering filtering = texture_filtering::bilinear) const;

    ///
    /// Similar to sample, but remaps the output values from [0, 255] to [0, 1] if the pixel data is
    /// stored as 8-bit integers.
    ///
    /// @param[in]  u               First sampling coordinate.
    /// @param[in]  v               Second sampling coordinate.
    /// @param[in]  filtering       Filtering scheme (nearest or bilinear).
    ///
    /// @tparam     TexcoordScalar  Texture coordinate type.
    /// @tparam     Scalar          Pixel scalar type. Must be a floating point type.
    /// @tparam     NumChannels     Number of color channels in the pixel.
    ///
    /// @return     Pixel data.
    ///
    template <typename TexcoordScalar, typename Scalar, uint32_t NumChannels>
    typename PixelTraits<Scalar, NumChannels>::Pixel sample_float(
        const TexcoordScalar u,
        const TexcoordScalar v,
        const texture_filtering filtering = texture_filtering::bilinear) const;

    /// Serialization.
    /// Caution: TessellationOptions::facet_budget_callback is not serialized
    template <typename Archive>
    void serialize_impl(Archive& ar);

protected:
    template <
        typename TexcoordScalar,
        typename Scalar,
        uint32_t NumChannels,
        typename InternalScalar,
        uint32_t InternalNumChannels>
    typename PixelTraits<Scalar, NumChannels>::Pixel sample(
        const TexcoordScalar u,
        const TexcoordScalar v,
        const texture_filtering filtering = texture_filtering::bilinear) const;

protected:
    size_t m_width = 0;
    size_t m_height = 0;
    size_t m_row_byte_stride = 0;
    precision_semantic m_pixel_precision = precision_semantic::single_p;
    color_space m_color_space = color_space::linear;
    texture_format m_tex_format = texture_format::rgba;
    wrap_mode m_wrap_u = wrap_mode::clamp;
    wrap_mode m_wrap_v = wrap_mode::clamp;
    image_storage_format m_storage_format = image_storage_format::first_pixel_row_at_top;

    /// Raw pixel data pointer. Assumed ownership by host, host's responsibility to keep it valid as long as Lagrange has access to it.
    const void* m_pixel_data = nullptr;

    std::vector<unsigned char> m_local_pixel_data;
};

template <typename Scalar, uint32_t NumChannels>
struct RawInputImage::PixelTraits
{
    static_assert(
        0u < NumChannels && NumChannels <= (static_cast<uint32_t>(texture_format::max) >> 8u));
    static_assert(
        std::is_same<unsigned char, Scalar>::value || std::is_same<Eigen::half, Scalar>::value ||
        std::is_same<float, Scalar>::value || std::is_same<double, Scalar>::value);

    using Pixel = Eigen::Matrix<Scalar, NumChannels, 1>;

    static Pixel zero() { return Pixel::Zero(); }

    static Scalar coeff(const Pixel& p, const size_t i)
    {
        assert(i < NumChannels);
        return p[i];
    }

    static Scalar& coeff(Pixel& p, const size_t i)
    {
        assert(i < NumChannels);
        return p[i];
    }
};

template <typename Scalar>
struct RawInputImage::PixelTraits<Scalar, 1>
{
    static_assert(
        std::is_same<unsigned char, Scalar>::value || std::is_same<Eigen::half, Scalar>::value ||
        std::is_same<float, Scalar>::value || std::is_same<double, Scalar>::value);

    using Pixel = Scalar;

    static Pixel zero() { return static_cast<Scalar>(0); }

    static Scalar coeff(const Pixel& p, [[maybe_unused]] const size_t i)
    {
        assert(0 == i);
        return p;
    }

    static Scalar& coeff(Pixel& p, [[maybe_unused]] const size_t i)
    {
        assert(0 == i);
        return p;
    }
};

/// Serialization of RawInputImage
template <typename Archive>
void serialize(lagrange::image::RawInputImage& image, Archive& ar)
{
    image.serialize_impl(ar);
}

/// Wrap a void * data into a Linear 4-component image. Pixel memory ownership is not transferred.
LA_IMAGE_API RawInputImage
make_default_rgba_image(std::size_t i_width, std::size_t i_height, const void* i_pixels);

/// Wrap a void * data into a Linear 1-component image. Pixel memory ownership is not transferred.
LA_IMAGE_API RawInputImage
make_default_luminance_image(std::size_t i_width, std::size_t i_height, const void* i_pixels);

/// wrap uv coordinate
template <typename T>
T wrap_uv(const T u, const RawInputImage::wrap_mode m)
{
    if (RawInputImage::wrap_mode::clamp == m) {
        return std::clamp(u, static_cast<T>(0), static_cast<T>(1));
    } else if (RawInputImage::wrap_mode::repeat == m) {
        return u - std::floor(u);
    } else if (RawInputImage::wrap_mode::mirror == m) {
        const auto f = static_cast<int>(std::floor(u));
        auto _u = u - static_cast<T>(f);
        if (f & 1) {
            _u = static_cast<T>(1) - _u;
        }
        return _u;
    }
    assert(0);
    return u;
}

} // namespace image
} // namespace lagrange

#include <lagrange/image/RawInputImage.impl.h>

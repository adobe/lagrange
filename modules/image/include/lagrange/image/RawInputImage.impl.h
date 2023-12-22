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

#include <lagrange/image/RawInputImage.h>
#include <lagrange/utils/warning.h>

namespace lagrange {
namespace image {


/// Serialization implementation
template <typename Archive>
inline void RawInputImage::serialize_impl(Archive& ar)
{
    LA_IGNORE_SHADOW_WARNING_BEGIN

    // basic parameters
    ar.object([&](auto& ar) {
        ar("width") & m_width;
        ar("height") & m_height;
        ar("row_byte_stride") & m_row_byte_stride;
        ar("pixel_precision") & m_pixel_precision;
        ar("color_space") & m_color_space;
        ar("tex_format") & m_tex_format;
        ar("wrap_u") & m_wrap_u;
        ar("wrap_v") & m_wrap_v;
        ar("storage_format") & m_storage_format;

        // aligned with sizeof(SerializationStorageOfRawInputImage::Scalar)
        // because lerida does not support array of byte
        // using ArrayScalar = SerializationStorageOfRawInputImage::Scalar;
        using ArrayScalar = int32_t;
        const bool padding_required = (0 != ((get_row_stride() * m_height) % sizeof(ArrayScalar)));
        const size_t aligned_image_size =
            get_row_stride() * m_height / sizeof(ArrayScalar) + (padding_required ? 1 : 0);

        // create a buffer pixel storage
        std::vector<unsigned char> buf(aligned_image_size * sizeof(ArrayScalar));
        ArrayScalar* data = reinterpret_cast<ArrayScalar*>(buf.data());
        if (ar.is_input()) {
            set_pixel_data_buffer(std::move(buf));
        } else {
            std::copy_n(
                static_cast<const unsigned char*>(get_pixel_data()) - get_pixel_data_offset(),
                get_row_stride() * m_height,
                buf.data());
        }

        // serialize pixels
        ar("pixels").template array_of<ArrayScalar>(
            aligned_image_size, // <-- only used during serialization
            [&](size_t /*size*/) { // <-- only used during deserialization
                // memory is already allocated above
            },
            [&](size_t i, auto& ar) { ar& data[i]; });
    });

    LA_IGNORE_SHADOW_WARNING_END
}

template <
    typename TexcoordScalar,
    typename Scalar,
    uint32_t NumChannels,
    typename InternalScalar,
    uint32_t InternalNumChannels>
typename RawInputImage::PixelTraits<Scalar, NumChannels>::Pixel RawInputImage::sample(
    const TexcoordScalar u,
    const TexcoordScalar v,
    const texture_filtering filtering) const
{
    using ReturnTraits = PixelTraits<Scalar, NumChannels>;
    using ReturnPixel = typename ReturnTraits::Pixel;
    using InternalTraits = PixelTraits<InternalScalar, InternalNumChannels>;
    using InternalPixel = typename InternalTraits::Pixel;

    ReturnPixel rtn = ReturnTraits::zero();

    // const for TexcoordScalar
    const TexcoordScalar _0 = static_cast<TexcoordScalar>(0);
    const TexcoordScalar _1 = static_cast<TexcoordScalar>(1);
    const TexcoordScalar _05 = static_cast<TexcoordScalar>(0.5);

    // static sanity check
    static_assert(
        std::is_same<TexcoordScalar, float>::value || std::is_same<TexcoordScalar, double>::value,
        "RawInputImage::sample, texcoord must be float or double");
    static_assert(
        std::is_same<Scalar, unsigned char>::value || std::is_same<Scalar, Eigen::half>::value ||
            std::is_same<Scalar, float>::value || std::is_same<Scalar, double>::value,
        "RawInputImage::sample, unsupported Scalar");
    static_assert(
        0u < NumChannels && NumChannels <= (static_cast<uint32_t>(texture_format::max) >> 8u),
        "RawInputImage::sample, unsupported NumChannels");
    static_assert(
        std::is_same<InternalScalar, unsigned char>::value ||
            std::is_same<InternalScalar, Eigen::half>::value ||
            std::is_same<InternalScalar, float>::value ||
            std::is_same<InternalScalar, double>::value,
        "RawInputImage::sample, unsupported InternalScalar");
    static_assert(
        0u < InternalNumChannels &&
            InternalNumChannels <= (static_cast<uint32_t>(texture_format::max) >> 8u),
        "RawInputImage::sample, unsupported InternalNumChannels");

    // runtime sanity check
    if (sizeof(InternalScalar) != get_size_precision()) {
        throw std::runtime_error("RawInputImage::sample, INTERNAL_PRECISION is incorrect!");
    }
    if (InternalNumChannels != get_num_channels()) {
        throw std::runtime_error("RawInputImage::sample, InternalNumChannels is incorrect!");
    }

    // something have not been implemented yet
    if (std::is_same<Scalar, Eigen::half>::value ||
        std::is_same<InternalScalar, Eigen::half>::value) {
        throw std::runtime_error("RawInputImage::sample, half is not implemented yet!");
    }
    if (color_space::sRGB == m_color_space) {
        throw std::runtime_error("RawInputImage::sample, sRGB is not implemented yet!");
    }

    // wrap coord
    auto wrap = [_0, _1](const TexcoordScalar c, const wrap_mode m) -> TexcoordScalar {
        if (_0 <= c && c <= _1) {
            return c;
        } else if (wrap_mode::repeat == m) {
            return c - std::floor(c);
        } else if (wrap_mode::clamp == m) {
            return std::clamp(c, _0, _1);
        } else if (wrap_mode::mirror == m) {
            const auto f = std::floor(c);
            if (static_cast<int>(f) & 1) {
                return _1 - (c - f);
            } else {
                return c - f;
            }
        } else {
            throw std::runtime_error("unknown wrap_mode!");
            return c;
        }
    };
    auto _u = wrap(u, m_wrap_u);
    auto _v = wrap(v, m_wrap_v);
    assert(_0 <= _u && _u <= _1);
    assert(_0 <= _v && _v <= _1);

    // v is bottom up in image space, convert it to memory space if storage is topdown
    if (image_storage_format::first_pixel_row_at_top == m_storage_format) {
        _v = _1 - _v;
    }

    // memory
    const auto size_pixel = get_size_pixel();
    const auto row_stride = get_row_stride();
    const auto ptr = static_cast<const unsigned char*>(get_pixel_data()) - get_pixel_data_offset();
    auto get_pixel = [&](size_t x, size_t y) -> InternalPixel {
        assert(x < m_width && y < m_height);
        return *reinterpret_cast<const InternalPixel*>(ptr + x * size_pixel + y * row_stride);
    };

    // sampling
    const auto x_coord = _u * static_cast<TexcoordScalar>(m_width);
    const auto y_coord = _v * static_cast<TexcoordScalar>(m_height);
    const auto min_num_channels = std::min(NumChannels, InternalNumChannels);
    if (filtering == texture_filtering::nearest) {
        const auto x = std::clamp(
            static_cast<size_t>(x_coord),
            static_cast<size_t>(0),
            static_cast<size_t>(m_width - 1));
        const auto y = std::clamp(
            static_cast<size_t>(y_coord),
            static_cast<size_t>(0),
            static_cast<size_t>(m_height - 1));
        const auto pix = get_pixel(x, y);
        for (uint32_t i = 0; i < min_num_channels; ++i) {
            ReturnTraits::coeff(rtn, i) = static_cast<Scalar>(InternalTraits::coeff(pix, i));
        }
    } else if (filtering == texture_filtering::bilinear) {
        auto sample_coord = [=](const TexcoordScalar coord, const size_t size, const wrap_mode wrap_)
            -> std::tuple<size_t, size_t, TexcoordScalar> {
            assert(_0 <= coord && coord <= static_cast<TexcoordScalar>(size));
            size_t coord0, coord1;
            TexcoordScalar t;
            if (coord <= _05) {
                coord0 = wrap_mode::repeat == wrap_ ? size - 1 : 0;
                coord1 = 0;
                t = _05 + coord;
            } else if (coord + _05 >= static_cast<TexcoordScalar>(size)) {
                coord0 = size - 1;
                coord1 = wrap_mode::repeat == wrap_ ? 0 : size - 1;
                t = coord - (static_cast<TexcoordScalar>(coord0) + _05);
            } else {
                assert(1 < size);
                coord0 = std::min(size - 2, static_cast<size_t>(coord - _05));
                coord1 = coord0 + 1;
                t = coord - (static_cast<TexcoordScalar>(coord0) + _05);
            }
            return std::make_tuple(coord0, coord1, t);
        };
        const auto sample_x = sample_coord(x_coord, m_width, m_wrap_u);
        const auto sample_y = sample_coord(y_coord, m_height, m_wrap_v);
        InternalPixel pix[4] = {
            get_pixel(std::get<0>(sample_x), std::get<0>(sample_y)),
            get_pixel(std::get<1>(sample_x), std::get<0>(sample_y)),
            get_pixel(std::get<0>(sample_x), std::get<1>(sample_y)),
            get_pixel(std::get<1>(sample_x), std::get<1>(sample_y))};
        TexcoordScalar weight[4] = {
            (_1 - std::get<2>(sample_x)) * (_1 - std::get<2>(sample_y)),
            std::get<2>(sample_x) * (_1 - std::get<2>(sample_y)),
            (_1 - std::get<2>(sample_x)) * std::get<2>(sample_y),
            std::get<2>(sample_x) * std::get<2>(sample_y)};
        for (uint32_t i = 0; i < min_num_channels; ++i) {
            TexcoordScalar sum = _0;
            for (int j = 0; j < 4; ++j) {
                sum += static_cast<TexcoordScalar>(InternalTraits::coeff(pix[j], i)) * weight[j];
            }
            ReturnTraits::coeff(rtn, i) = static_cast<Scalar>(sum);
        }
    } else {
        throw std::runtime_error("RawInputImage::sample, unknown filtering type!");
    }

    return rtn;
}

template <typename TexcoordScalar, typename Scalar, uint32_t NumChannels>
typename RawInputImage::PixelTraits<Scalar, NumChannels>::Pixel RawInputImage::sample(
    const TexcoordScalar u,
    const TexcoordScalar v,
    const texture_filtering filtering) const
{
    using ReturnTraits = PixelTraits<Scalar, NumChannels>;
    using ReturnPixel = typename ReturnTraits::Pixel;

    ReturnPixel rtn = ReturnTraits::zero();

    // the function is implemented assuming the max channels is 4
    static_assert(
        4u == (static_cast<uint32_t>(texture_format::max) >> 8u),
        "the max channels are not 4 any more, need to update the following code");

    const auto channels = get_num_channels();

#define LA_RAWINPUTIMAGE_SAMPLE_IMPL(P, C, V)                                      \
    if (P == m_pixel_precision && C == channels) {                                 \
        return sample<TexcoordScalar, Scalar, NumChannels, V, C>(u, v, filtering); \
    }

    LA_RAWINPUTIMAGE_SAMPLE_IMPL(precision_semantic::byte_p, 1u, unsigned char)
    LA_RAWINPUTIMAGE_SAMPLE_IMPL(precision_semantic::byte_p, 2u, unsigned char)
    LA_RAWINPUTIMAGE_SAMPLE_IMPL(precision_semantic::byte_p, 3u, unsigned char)
    LA_RAWINPUTIMAGE_SAMPLE_IMPL(precision_semantic::byte_p, 4u, unsigned char)

    LA_RAWINPUTIMAGE_SAMPLE_IMPL(precision_semantic::half_p, 1u, Eigen::half)
    LA_RAWINPUTIMAGE_SAMPLE_IMPL(precision_semantic::half_p, 2u, Eigen::half)
    LA_RAWINPUTIMAGE_SAMPLE_IMPL(precision_semantic::half_p, 3u, Eigen::half)
    LA_RAWINPUTIMAGE_SAMPLE_IMPL(precision_semantic::half_p, 4u, Eigen::half)

    LA_RAWINPUTIMAGE_SAMPLE_IMPL(precision_semantic::single_p, 1u, float)
    LA_RAWINPUTIMAGE_SAMPLE_IMPL(precision_semantic::single_p, 2u, float)
    LA_RAWINPUTIMAGE_SAMPLE_IMPL(precision_semantic::single_p, 3u, float)
    LA_RAWINPUTIMAGE_SAMPLE_IMPL(precision_semantic::single_p, 4u, float)

    LA_RAWINPUTIMAGE_SAMPLE_IMPL(precision_semantic::double_p, 1u, double)
    LA_RAWINPUTIMAGE_SAMPLE_IMPL(precision_semantic::double_p, 2u, double)
    LA_RAWINPUTIMAGE_SAMPLE_IMPL(precision_semantic::double_p, 3u, double)
    LA_RAWINPUTIMAGE_SAMPLE_IMPL(precision_semantic::double_p, 4u, double)

#undef LA_RAWINPUTIMAGE_SAMPLE_IMPL
    throw std::runtime_error(
        "RawInputImage::sample, cannot deduce InternalScalar or InternalNumChannels!");
    return rtn;
}

template <typename TexcoordScalar, typename Scalar, uint32_t NumChannels>
typename RawInputImage::PixelTraits<Scalar, NumChannels>::Pixel RawInputImage::sample_float(
    const TexcoordScalar u,
    const TexcoordScalar v,
    const texture_filtering filtering) const
{
    static_assert(std::is_floating_point_v<Scalar>, "Pixel scalar type must be floating point");

    auto pixel = sample<TexcoordScalar, Scalar, NumChannels>(u, v, filtering);
    return (get_pixel_precision() == precision_semantic::byte_p ? pixel / Scalar(255) : pixel);
}

} // namespace image
} // namespace lagrange

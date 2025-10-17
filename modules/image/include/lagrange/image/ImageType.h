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

#include <Eigen/Eigen>

#include <algorithm>
#include <type_traits>

namespace lagrange {
namespace image {

enum class ImagePrecision : unsigned int {
    uint8,
    int8,
    uint32,
    int32,
    float32,
    float64,
    float16,
    unknown,
};
enum class ImageChannel : unsigned int {
    one = 1,
    three = 3,
    four = 4,
    unknown,
};

template <typename T>
struct ImageTraits;

// to convert the value of image, included in ImagePrecision,
// it is not the same as static_cast:
//  * unsigned char <==> float/double : [0, 255] <==> [-1.0, 1.0]
//  * others : clamp to avoid overflow
template <typename VALUE_SRC, typename VALUE_DST>
inline VALUE_DST convert_channel_value(VALUE_SRC val);

template <typename PIX_SRC, typename PIX_DST>
struct convert_image_pixel
{
    void operator()(const PIX_SRC& src, PIX_DST& dst) const;
};


template <typename TYPE>
struct ImageTraits
{
    static constexpr ImagePrecision precision = ImagePrecision::unknown;
    static constexpr ImageChannel channel = ImageChannel::unknown;
};

#ifdef LAGRANGE_IMAGE_TRAITS
static_assert(false, "LAGRANGE_IMAGE_TRAITS was defined somewhere else")
#elif LAGRANGE_IMAGE_COMMA
static_assert(false, "LAGRANGE_IMAGE_COMMA was defined somewhere else")
#else

    #define LAGRANGE_IMAGE_TRAITS(TYPE, VALUE, SIZE_OF_VALUE, PRECISION, CHANNEL)  \
        template <>                                                                \
        struct ImageTraits<TYPE>                                                   \
        {                                                                          \
            typedef VALUE TValue;                                                  \
            static constexpr size_t value_size = SIZE_OF_VALUE;                    \
            static constexpr ImagePrecision precision = ImagePrecision::PRECISION; \
            static constexpr ImageChannel channel = ImageChannel::CHANNEL;         \
        };

    #define LAGRANGE_IMAGE_COMMA ,
LAGRANGE_IMAGE_TRAITS(unsigned char, unsigned char, 1, uint8, one)
LAGRANGE_IMAGE_TRAITS(
    Eigen::Matrix<unsigned char LAGRANGE_IMAGE_COMMA 3 LAGRANGE_IMAGE_COMMA 1>,
    unsigned char,
    3,
    uint8,
    three)
LAGRANGE_IMAGE_TRAITS(
    Eigen::Matrix<unsigned char LAGRANGE_IMAGE_COMMA 4 LAGRANGE_IMAGE_COMMA 1>,
    unsigned char,
    4,
    uint8,
    four)
LAGRANGE_IMAGE_TRAITS(char, char, 1, int8, one)
LAGRANGE_IMAGE_TRAITS(
    Eigen::Matrix<char LAGRANGE_IMAGE_COMMA 3 LAGRANGE_IMAGE_COMMA 1>,
    char,
    3,
    int8,
    three)
LAGRANGE_IMAGE_TRAITS(
    Eigen::Matrix<char LAGRANGE_IMAGE_COMMA 4 LAGRANGE_IMAGE_COMMA 1>,
    char,
    4,
    int8,
    four)
LAGRANGE_IMAGE_TRAITS(unsigned int, unsigned int, 1, uint32, one)
LAGRANGE_IMAGE_TRAITS(
    Eigen::Matrix<unsigned int LAGRANGE_IMAGE_COMMA 3 LAGRANGE_IMAGE_COMMA 1>,
    unsigned int,
    3,
    uint32,
    three)
LAGRANGE_IMAGE_TRAITS(
    Eigen::Matrix<unsigned int LAGRANGE_IMAGE_COMMA 4 LAGRANGE_IMAGE_COMMA 1>,
    unsigned int,
    4,
    uint32,
    four)
LAGRANGE_IMAGE_TRAITS(int, int, 1, int32, one)
LAGRANGE_IMAGE_TRAITS(Eigen::Vector3i, int, 3, int32, three)
LAGRANGE_IMAGE_TRAITS(Eigen::Vector4i, int, 4, int32, four)
LAGRANGE_IMAGE_TRAITS(float, float, 1, float32, one)
LAGRANGE_IMAGE_TRAITS(Eigen::Vector3f, float, 3, float32, three)
LAGRANGE_IMAGE_TRAITS(Eigen::Vector4f, float, 4, float32, four)
LAGRANGE_IMAGE_TRAITS(double, double, 1, float64, one)
LAGRANGE_IMAGE_TRAITS(Eigen::Vector3d, double, 3, float64, three)
LAGRANGE_IMAGE_TRAITS(Eigen::Vector4d, double, 4, float64, four)
LAGRANGE_IMAGE_TRAITS(Eigen::half, Eigen::half, 1, float16, one)
LAGRANGE_IMAGE_TRAITS(
    Eigen::Matrix<Eigen::half LAGRANGE_IMAGE_COMMA 3 LAGRANGE_IMAGE_COMMA 1>,
    Eigen::half,
    3,
    float16,
    three)
LAGRANGE_IMAGE_TRAITS(
    Eigen::Matrix<Eigen::half LAGRANGE_IMAGE_COMMA 4 LAGRANGE_IMAGE_COMMA 1>,
    Eigen::half,
    4,
    float16,
    four)

    #undef LAGRANGE_IMAGE_TRAITS
    #undef LAGRANGE_IMAGE_COMMA
#endif

    template <typename VALUE_SRC, typename VALUE_DST>
    inline VALUE_DST convert_channel_value(VALUE_SRC val)
{
    // convert between same type
    if constexpr (std::is_same<VALUE_SRC, VALUE_DST>::value) {
        return val;
    }
    // convert from unsigned char and float/double
    else if constexpr (
        std::is_same<VALUE_SRC, unsigned char>::value && std::is_floating_point<VALUE_DST>::value) {
        return static_cast<VALUE_DST>(val) /
               static_cast<VALUE_DST>(std::numeric_limits<unsigned char>::max());
    }
    // convert from float/double to unsigned char
    else if constexpr (
        std::is_floating_point<VALUE_SRC>::value && std::is_same<VALUE_DST, unsigned char>::value) {
        return static_cast<unsigned char>(
            std::clamp(val, static_cast<VALUE_SRC>(0), static_cast<VALUE_SRC>(1)) *
            static_cast<VALUE_SRC>(std::numeric_limits<unsigned char>::max()));
    } else {
        // clamping, prepare to convert from signed to unsigned
        if constexpr (std::is_signed<VALUE_SRC>::value && !std::is_signed<VALUE_DST>::value) {
            val = std::max(val, static_cast<VALUE_SRC>(0));
        }
        // convert between integral types, and it may overflow
        if constexpr (
            (std::is_integral<VALUE_SRC>::value && std::is_integral<VALUE_DST>::value) &&
            (sizeof(VALUE_SRC) > sizeof(VALUE_DST) ||
             (sizeof(VALUE_SRC) == sizeof(VALUE_DST) && std::is_signed<VALUE_DST>::value))) {
            return static_cast<VALUE_DST>(
                std::min(val, static_cast<VALUE_SRC>(std::numeric_limits<VALUE_DST>::max())));
        }
        // the rest
        else {
            return static_cast<VALUE_DST>(val);
        }
    }
}

template <typename PIX_SRC, typename PIX_DST>
inline void convert_image_pixel<PIX_SRC, PIX_DST>::operator()(const PIX_SRC& src, PIX_DST& dst)
    const
{
    using V_SRC = typename ImageTraits<PIX_SRC>::TValue;
    using V_DST = typename ImageTraits<PIX_DST>::TValue;
    constexpr size_t L_SRC = static_cast<size_t>(ImageTraits<PIX_SRC>::channel);
    constexpr size_t L_DST = static_cast<size_t>(ImageTraits<PIX_DST>::channel);
    // convert [1] to [1]
    if constexpr (1 == L_SRC && 1 == L_DST) {
        dst = convert_channel_value<V_SRC, V_DST>(src);
    }
    // convert [1] to [n]
    else if constexpr (1 == L_SRC) {
        dst(0) = convert_channel_value<V_SRC, V_DST>(src);
        for (size_t i = 1; i < L_DST; ++i) {
            dst(i) = static_cast<V_DST>(0);
        }
        if constexpr (3 <= L_DST) {
            dst(1) = dst(0);
            dst(2) = dst(0);
        }
        if constexpr (4 == L_DST) {
            if constexpr (std::is_floating_point<V_DST>::value) {
                dst(3) = static_cast<V_DST>(1);
            } else if constexpr (!std::is_signed<V_DST>::value && 1 == sizeof(V_DST)) {
                dst(3) = std::numeric_limits<V_DST>::max();
            } else {
                dst(3) = static_cast<V_DST>(0);
            }
        }
    }
    // convert [n] to [1]
    else if constexpr (1 == L_DST) {
        dst = convert_channel_value<V_SRC, V_DST>(src(0));
    }
    // convert [m] to [n]
    else {
        constexpr size_t L_MIN = std::min(L_SRC, L_DST);
        for (size_t i = 0; i < L_MIN; ++i) {
            dst(i) = convert_channel_value<V_SRC, V_DST>(src(i));
        }
        if constexpr (3 == L_MIN && 4 == L_DST) {
            if constexpr (std::is_floating_point<V_DST>::value) {
                dst(3) = static_cast<V_DST>(1);
            } else if constexpr (!std::is_signed<V_DST>::value && 1 == sizeof(V_DST)) {
                dst(3) = std::numeric_limits<V_DST>::max();
            } else {
                dst(3) = static_cast<V_DST>(0);
            }
        } else {
            for (size_t i = L_MIN; i < L_DST; ++i) {
                dst(i) = static_cast<V_DST>(0);
            }
        }
    }
}
} // namespace image
} // namespace lagrange

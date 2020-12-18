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

#include <cstdint>

namespace lagrange {
namespace experimental {

enum class ScalarEnum : uint8_t {
    INT8 = 0,
    INT16 = 1,
    INT32 = 2,
    INT64 = 3,
    UINT8 = 4,
    UINT16 = 5,
    UINT32 = 6,
    UINT64 = 7,
    FLOAT = 8,
    DOUBLE = 9,
    LONG_DOUBLE = 10,
    SIZET = 11,
    LONG = 12,
    UNKNOWN = 255
};

template <typename T, typename T2 = void>
struct ScalarToEnum
{
    static constexpr const char* name = "unknown";
};

template <>
struct ScalarToEnum<int8_t, void>
{
    static constexpr ScalarEnum value = ScalarEnum::INT8;
    static constexpr const char* name = "int8_t";
};

template <>
struct ScalarToEnum<int16_t, void>
{
    static constexpr ScalarEnum value = ScalarEnum::INT16;
    static constexpr const char* name = "int16_t";
};

template <>
struct ScalarToEnum<int32_t, void>
{
    static constexpr ScalarEnum value = ScalarEnum::INT32;
    static constexpr const char* name = "int32_t";
};

template <>
struct ScalarToEnum<int64_t, void>
{
    static constexpr ScalarEnum value = ScalarEnum::INT64;
    static constexpr const char* name = "int64_t";
};

template <>
struct ScalarToEnum<uint8_t, void>
{
    static constexpr ScalarEnum value = ScalarEnum::UINT8;
    static constexpr const char* name = "uint8_t";
};

template <>
struct ScalarToEnum<uint16_t, void>
{
    static constexpr ScalarEnum value = ScalarEnum::UINT16;
    static constexpr const char* name = "uint16_t";
};

template <>
struct ScalarToEnum<uint32_t, void>
{
    static constexpr ScalarEnum value = ScalarEnum::UINT32;
    static constexpr const char* name = "uint32_t";
};

template <>
struct ScalarToEnum<uint64_t, void>
{
    static constexpr ScalarEnum value = ScalarEnum::UINT64;
    static constexpr const char* name = "uint64_t";
};

template <>
struct ScalarToEnum<float, void>
{
    static constexpr ScalarEnum value = ScalarEnum::FLOAT;
    static constexpr const char* name = "float";
};

template <>
struct ScalarToEnum<double, void>
{
    static constexpr ScalarEnum value = ScalarEnum::DOUBLE;
    static constexpr const char* name = "double";
};

template <>
struct ScalarToEnum<long double, void>
{
    static constexpr ScalarEnum value = ScalarEnum::LONG_DOUBLE;
    static constexpr const char* name = "long double";
};

template <typename T>
struct ScalarToEnum<
    T,
    std::enable_if_t<
        std::is_same<T, size_t>::value && !std::is_same<T, uint32_t>::value &&
        !std::is_same<T, uint64_t>::value>>
{
    static constexpr ScalarEnum value = ScalarEnum::SIZET;
    static constexpr const char* name = "size_t";
};

template <typename T>
struct ScalarToEnum<
    T,
    std::enable_if_t<
        std::is_same<T, long>::value && !std::is_same<T, int32_t>::value &&
        !std::is_same<T, int64_t>::value>>
{
    static constexpr ScalarEnum value = ScalarEnum::LONG;
    static constexpr const char* name = "long";
};

template <typename T>
constexpr ScalarEnum ScalarToEnum_v = ScalarToEnum<T>::value;


template <ScalarEnum type>
struct EnumToScalar
{
};

template <>
struct EnumToScalar<ScalarEnum::INT8>
{
    using type = int8_t;
};

template <>
struct EnumToScalar<ScalarEnum::INT16>
{
    using type = int16_t;
};

template <>
struct EnumToScalar<ScalarEnum::INT32>
{
    using type = int32_t;
};

template <>
struct EnumToScalar<ScalarEnum::INT64>
{
    using type = int64_t;
};

template <>
struct EnumToScalar<ScalarEnum::UINT8>
{
    using type = uint8_t;
};

template <>
struct EnumToScalar<ScalarEnum::UINT16>
{
    using type = uint16_t;
};

template <>
struct EnumToScalar<ScalarEnum::UINT32>
{
    using type = uint32_t;
};

template <>
struct EnumToScalar<ScalarEnum::UINT64>
{
    using type = uint64_t;
};

template <>
struct EnumToScalar<ScalarEnum::FLOAT>
{
    using type = float;
};

template <>
struct EnumToScalar<ScalarEnum::DOUBLE>
{
    using type = double;
};

template <>
struct EnumToScalar<ScalarEnum::LONG_DOUBLE>
{
    using type = long double;
};

template <>
struct EnumToScalar<ScalarEnum::SIZET>
{
    using type = size_t;
};

template <>
struct EnumToScalar<ScalarEnum::LONG>
{
    using type = long;
};

template <ScalarEnum T>
using EnumToScalar_t = typename EnumToScalar<T>::type;

} // namespace experimental
} // namespace lagrange

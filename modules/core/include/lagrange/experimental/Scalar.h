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
};

template <typename T, typename T2 = void>
struct ScalarToEnum
{
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
    static_assert(sizeof(float) == 4, "sizeof(float) == 4");
    static constexpr ScalarEnum value = ScalarEnum::FLOAT;
    static constexpr const char* name = "float";
};

template <>
struct ScalarToEnum<double, void>
{
    static_assert(sizeof(double) == 8, "sizeof(double) == 8");
    static constexpr ScalarEnum value = ScalarEnum::DOUBLE;
    static constexpr const char* name = "double";
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

template <ScalarEnum T>
using EnumToScalar_t = typename EnumToScalar<T>::type;

/**
 * Look up ScalarEnum name at run time.
 */
inline std::string enum_to_name(ScalarEnum t)
{
    switch (t) {
    case ScalarEnum::INT8: return ScalarToEnum<int8_t>::name;
    case ScalarEnum::INT16: return ScalarToEnum<int16_t>::name;
    case ScalarEnum::INT32: return ScalarToEnum<int32_t>::name;
    case ScalarEnum::INT64: return ScalarToEnum<int64_t>::name;
    case ScalarEnum::UINT8: return ScalarToEnum<uint8_t>::name;
    case ScalarEnum::UINT16: return ScalarToEnum<uint16_t>::name;
    case ScalarEnum::UINT32: return ScalarToEnum<uint32_t>::name;
    case ScalarEnum::UINT64: return ScalarToEnum<uint64_t>::name;
    case ScalarEnum::FLOAT: return ScalarToEnum<float>::name;
    case ScalarEnum::DOUBLE: return ScalarToEnum<double>::name;
    default: return "unknown";
    }
}

} // namespace experimental
} // namespace lagrange

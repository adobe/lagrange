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
#include <lagrange/utils/assert.h>
#include <lagrange/utils/warning.h>
#include <cassert>

LA_IGNORE_SHADOW_WARNING_BEGIN
#if !defined(__clang__) && defined(__gnuc__)
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

namespace lagrange {
namespace experimental {
namespace internal {

template <typename T>
struct ArraySerialization
{
    template <typename Archive>
    static void serialize(ArrayBase* arr, Archive& ar)
    {
        if (arr->is_row_major()) {
            using EigenType = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
            auto M = arr->template view<EigenType>();
            ar& M;
        } else {
            using EigenType = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor>;
            auto M = arr->template view<EigenType>();
            ar& M;
        }
    }

    template <int StorageOrder, typename Archive>
    static std::unique_ptr<ArrayBase> deserialize(Archive& ar)
    {
        using EigenType = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, StorageOrder>;
        EigenType matrix;
        ar& matrix;
        return std::make_unique<EigenArray<EigenType>>(std::move(matrix));
    }
};

template <typename Archive>
void serialize_array(ArrayBase* arr, Archive& ar)
{
    assert(!ar.is_input());
    enum { SCALAR_TYPE = 0, IS_ROW_MAJOR = 1, DATA = 2 };

    ar.object([&](auto& ar) {
        auto stype = arr->get_scalar_type();
        auto is_row_major = arr->is_row_major();
        ar("scalar_type", SCALAR_TYPE) & stype;
        ar("is_row_major", IS_ROW_MAJOR) & is_row_major;

        auto data_ar = ar("data", DATA);

        switch (stype) {
#define LAGRANGE_SERIALIZATION_ADD_CASE(type)                              \
    case type:                                                             \
        ArraySerialization<EnumToScalar_t<type>>::serialize(arr, data_ar); \
        break;
            LAGRANGE_SERIALIZATION_ADD_CASE(ScalarEnum::INT8)
            LAGRANGE_SERIALIZATION_ADD_CASE(ScalarEnum::INT16)
            LAGRANGE_SERIALIZATION_ADD_CASE(ScalarEnum::INT32)
            LAGRANGE_SERIALIZATION_ADD_CASE(ScalarEnum::INT64)
            LAGRANGE_SERIALIZATION_ADD_CASE(ScalarEnum::UINT8)
            LAGRANGE_SERIALIZATION_ADD_CASE(ScalarEnum::UINT16)
            LAGRANGE_SERIALIZATION_ADD_CASE(ScalarEnum::UINT32)
            LAGRANGE_SERIALIZATION_ADD_CASE(ScalarEnum::UINT64)
            LAGRANGE_SERIALIZATION_ADD_CASE(ScalarEnum::FLOAT)
            LAGRANGE_SERIALIZATION_ADD_CASE(ScalarEnum::DOUBLE)
        default: throw std::runtime_error("Unsupported scalar type detected!");
#undef LAGRANGE_SERIALIZATION_ADD_CASE
        }
    });
}

template <typename Archive>
std::unique_ptr<ArrayBase> deserialize_array(Archive& ar)
{
    enum { SCALAR_TYPE = 0, IS_ROW_MAJOR = 1, DATA = 2 };
    std::unique_ptr<ArrayBase> ptr;

    ar.object([&](auto& ar) {
        ScalarEnum stype;
        bool is_row_major;
        ar("scalar_type", SCALAR_TYPE) & stype;
        ar("is_row_major", IS_ROW_MAJOR) & is_row_major;

        auto data_ar = ar("data", DATA);

        switch (stype) {
#define LAGRANGE_SERIALIZATION_ADD_CASE(type)                                                      \
    case type:                                                                                     \
        ptr =                                                                                      \
            is_row_major                                                                           \
                ? ArraySerialization<EnumToScalar_t<type>>::deserialize<Eigen::RowMajor>(data_ar)  \
                : ArraySerialization<EnumToScalar_t<type>>::deserialize<Eigen::ColMajor>(data_ar); \
        break;
            LAGRANGE_SERIALIZATION_ADD_CASE(ScalarEnum::INT8)
            LAGRANGE_SERIALIZATION_ADD_CASE(ScalarEnum::INT16)
            LAGRANGE_SERIALIZATION_ADD_CASE(ScalarEnum::INT32)
            LAGRANGE_SERIALIZATION_ADD_CASE(ScalarEnum::INT64)
            LAGRANGE_SERIALIZATION_ADD_CASE(ScalarEnum::UINT8)
            LAGRANGE_SERIALIZATION_ADD_CASE(ScalarEnum::UINT16)
            LAGRANGE_SERIALIZATION_ADD_CASE(ScalarEnum::UINT32)
            LAGRANGE_SERIALIZATION_ADD_CASE(ScalarEnum::UINT64)
            LAGRANGE_SERIALIZATION_ADD_CASE(ScalarEnum::FLOAT)
            LAGRANGE_SERIALIZATION_ADD_CASE(ScalarEnum::DOUBLE)
        default: throw std::runtime_error("Unsupported scalar type detected!");
        }
#undef LAGRANGE_SERIALIZATION_ADD_CASE
    });
    assert(ptr != nullptr);
    return ptr;
}

} // namespace internal

/**
 * This method provide serialization and deserialization capabilities to generic
 * array.
 *
 * @param array  Generic array pointer.
 * @param ar     An Archive object.
 */
template <typename Archive>
void serialize(std::shared_ptr<ArrayBase>& arr, Archive& ar)
{
    if (ar.is_input()) {
        arr = internal::deserialize_array(ar);
    } else {
        internal::serialize_array(arr.get(), ar);
    }
}

/**
 * This method provide serialization and deserialization capabilities to generic
 * array.
 *
 * @param array  Generic array pointer.
 * @param ar     An Archive object.
 */
template <typename Archive>
void serialize(std::unique_ptr<ArrayBase>& arr, Archive& ar)
{
    if (ar.is_input()) {
        arr = internal::deserialize_array(ar);
    } else {
        internal::serialize_array(arr.get(), ar);
    }
}

} // experimental
} // namespace lagrange

LA_IGNORE_SHADOW_WARNING_END

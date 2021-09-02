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

#include <lagrange/common.h>
#include <lagrange/experimental/Array.h>

namespace lagrange {
namespace experimental {

// `create_array` will take up ownership of the data, thus data is copied into
// the array data structure.
//
// In contrast, `wrap_with_array` will not take ownership of the data and no
// copy is performed.  However, it assumes data stays valid through its life
// time.

template <typename Derived>
std::unique_ptr<ArrayBase> create_array(const Eigen::MatrixBase<Derived>& matrix)
{
    using EvalType = std::decay_t<decltype(matrix.eval())>;
    return std::make_unique<EigenArray<EvalType>>(matrix);
}

template <typename Derived>
std::unique_ptr<ArrayBase> create_array(const Eigen::ArrayBase<Derived>& array)
{
    return create_array(array.matrix());
}

template <typename Derived>
std::unique_ptr<ArrayBase> create_array(Eigen::PlainObjectBase<Derived>&& matrix)
{
    static_assert(!std::is_const<Derived>::value, "Derived should not be const!");
#ifndef NDEBUG
    const void* ptr = matrix.data();
#endif
    auto r = std::make_unique<EigenArray<Derived>>(std::move(matrix.derived()));
#ifndef NDEBUG
    const void* ptr2 = r->data();
    LA_ASSERT(ptr == ptr2, "Data is copied when it should have been moved.");
#endif
    return r;
}

template <typename Scalar, typename Index, int Options = Eigen::RowMajor>
std::unique_ptr<ArrayBase> create_array(Scalar* values, Index rows, Index cols)
{
    using EigenType = Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Options>;
    return std::make_unique<EigenArray<EigenType>>(Eigen::Map<EigenType>(values, rows, cols));
}

static inline std::shared_ptr<ArrayBase> create_array(std::shared_ptr<ArrayBase> array_ptr)
{
    return array_ptr;
}

template <typename Derived>
std::unique_ptr<ArrayBase> wrap_with_array(Eigen::PlainObjectBase<Derived>& matrix)
{
    static_assert(!std::is_const<Derived>::value, "Derived should not be const!");
    return std::make_unique<EigenArrayRef<Derived>>(matrix);
}

template <typename Derived>
std::unique_ptr<const ArrayBase> wrap_with_array(const Eigen::PlainObjectBase<Derived>& matrix)
{
    static_assert(!std::is_const<Derived>::value, "Derived should not be const!");
    return std::make_unique<EigenArrayRef<const Derived>>(matrix);
}

template <typename Derived>
std::unique_ptr<ArrayBase> wrap_with_array(const Eigen::PlainObjectBase<Derived>&& /*matrix*/)
{
    static_assert(
        StaticAssertableBool<Derived>::False,
        "Lagrange Array cannot wrap around r-value Eigen matrix "
        "because its memory may be invalid after the creation.");
    return nullptr;
}

template <typename Derived, int Options, typename StrideType>
std::enable_if_t<!std::is_const<Derived>::value, std::unique_ptr<ArrayBase>> wrap_with_array(
    Eigen::Map<Derived, Options, StrideType>& matrix)
{
    static_assert(!std::is_const<Derived>::value, "Derived should not be const!");
    return std::make_unique<RawArray<
        typename Derived::Scalar,
        Derived::RowsAtCompileTime,
        Derived::ColsAtCompileTime,
        Derived::Options>>(matrix.data(), matrix.rows(), matrix.cols());
}

template <typename Derived, int Options, typename StrideType>
std::enable_if_t<std::is_const<Derived>::value, std::unique_ptr<const ArrayBase>> wrap_with_array(
    const Eigen::Map<Derived, Options, StrideType>& matrix)
{
    return std::make_unique<const RawArray<
        const typename Derived::Scalar,
        Derived::RowsAtCompileTime,
        Derived::ColsAtCompileTime,
        Derived::Options>>(matrix.data(), matrix.rows(), matrix.cols());
}

template <typename Derived, int Options, typename StrideType>
std::enable_if_t<!std::is_const<Derived>::value, std::unique_ptr<ArrayBase>> wrap_with_array(
    Eigen::Map<Derived, Options, StrideType>&& matrix)
{
    return std::make_unique<RawArray<
        typename Derived::Scalar,
        Derived::RowsAtCompileTime,
        Derived::ColsAtCompileTime,
        Derived::Options>>(matrix.data(), matrix.rows(), matrix.cols());
}

template <typename Derived, int Options, typename StrideType>
std::enable_if_t<std::is_const<Derived>::value, std::unique_ptr<const ArrayBase>> wrap_with_array(
    const Eigen::Map<Derived, Options, StrideType>&& matrix)
{
    return std::make_unique<const RawArray<
        const typename Derived::Scalar,
        Derived::RowsAtCompileTime,
        Derived::ColsAtCompileTime,
        Derived::Options>>(matrix.data(), matrix.rows(), matrix.cols());
}

template <
    typename Scalar,
    typename Index,
    int Rows = Eigen::Dynamic,
    int Cols = Eigen::Dynamic,
    int Options = Eigen::RowMajor>
std::unique_ptr<ArrayBase> wrap_with_array(Scalar* values, Index rows, Index cols)
{
    return std::make_unique<RawArray<Scalar, Rows, Cols, Options>>(values, rows, cols);
}

template <
    typename Scalar,
    typename Index,
    int Rows = Eigen::Dynamic,
    int Cols = Eigen::Dynamic,
    int Options = Eigen::RowMajor>
std::unique_ptr<const ArrayBase> wrap_with_array(const Scalar* values, Index rows, Index cols)
{
    return std::make_unique<const RawArray<const Scalar, Rows, Cols, Options>>(values, rows, cols);
}


} // namespace experimental
} // namespace lagrange

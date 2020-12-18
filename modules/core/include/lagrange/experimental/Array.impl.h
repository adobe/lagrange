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
#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for.h>

namespace lagrange {
namespace experimental {

template <typename Derived>
void ArrayBase::set(const Eigen::MatrixBase<Derived>& data)
{
    using EvalType = std::decay_t<decltype(data.eval())>;
    if (auto ptr = dynamic_cast<EigenArray<EvalType>*>(this)) {
        ptr->set(data);
    } else if (
        auto ptr2 = dynamic_cast<RawArray<
            typename EvalType::Scalar,
            EvalType::RowsAtCompileTime,
            EvalType::ColsAtCompileTime,
            EvalType::Options>*>(this)) {
        ptr2->set(data);
    } else if (auto ptr3 = dynamic_cast<EigenArrayRef<EvalType>*>(this)) {
        ptr3->set(data);
    } else if (is_compatible<EvalType>(true)) {
        // The Derived type is not an exact match to the true type, fall back to
        // brute force copying.
        if (EvalType::IsRowMajor == is_row_major()) {
            resize(data.rows(), data.cols());
            view<EvalType>() = data;
        } else {
            // Type is compatible except storage order.
            using TransposedType = std::decay_t<decltype(data.transpose().eval())>;
            LA_ASSERT(is_compatible<TransposedType>());
            resize(data.rows(), data.cols());
            view<TransposedType>() = data;
        }
    } else {
        throw std::runtime_error(string_format(
            "Unsupported type passed to ArrayBase::set().  Expecting {}",
            type_name()));
    }
}

template <typename Derived>
void ArrayBase::set(Eigen::MatrixBase<Derived>&& data)
{
    using EvalType = std::decay_t<decltype(data.eval())>;
    if (auto ptr = dynamic_cast<EigenArray<EvalType>*>(this)) {
        ptr->set(std::move(data.derived()));
    } else if (
        auto ptr2 = dynamic_cast<RawArray<
            typename EvalType::Scalar,
            EvalType::RowsAtCompileTime,
            EvalType::ColsAtCompileTime,
            EvalType::Options>*>(this)) {
        // Fall back to copying because RawArray cannot change its data memory
        // pointer.
        ptr2->set(data);
    } else if (auto ptr3 = dynamic_cast<EigenArrayRef<EvalType>*>(this)) {
        ptr3->set(std::move(data.derived()));
    } else if (is_compatible<EvalType>(true)) {
        // The Derived type is not an exact match to the true type, fall back to
        // brute force copying.
        if (EvalType::IsRowMajor == is_row_major()) {
            resize(data.rows(), data.cols());
            view<EvalType>() = data;
        } else {
            // Type is compatible except storage order.
            using TransposedType = std::decay_t<decltype(data.transpose().eval())>;
            LA_ASSERT(is_compatible<TransposedType>());
            resize(data.rows(), data.cols());
            view<TransposedType>() = std::move(data);
        }
    } else {
        throw std::runtime_error(string_format(
            "Unsupported type passed to ArrayBase::set().  Expecting {}",
            type_name()));
    }
}

template <typename _TargetType>
auto& ArrayBase::get()
{
    using TargetType = std::decay_t<_TargetType>;
    if (auto ptr = dynamic_cast<EigenArray<TargetType>*>(this)) {
        return ptr->get_ref();
    } else if (auto ptr2 = dynamic_cast<EigenArrayRef<TargetType>*>(this)) {
        return ptr2->get_ref();
    } else {
        throw std::runtime_error(string_format(
            "Unsupported type passed to ArrayBase::get().  Expecting {}",
            type_name()));
    }
}

template <typename _TargetType>
const auto& ArrayBase::get() const
{
    using TargetType = std::decay_t<_TargetType>;
    if (auto ptr = dynamic_cast<const EigenArray<TargetType>*>(this)) {
        return ptr->get_ref();
    } else if (auto ptr3 = dynamic_cast<const EigenArrayRef<TargetType>*>(this)) {
        return ptr3->get_ref();
    } else if (auto ptr4 = dynamic_cast<const EigenArrayRef<const TargetType>*>(this)) {
        return ptr4->get_ref();
    } else {
        throw std::runtime_error(string_format(
            "Unsupported type passed to ArrayBase::get().  Expecting {}",
            type_name()));
    }
}

template <typename TargetType>
bool ArrayBase::is_compatible(bool ignore_storage_order) const
{
    using Scalar = typename TargetType::Scalar;
    constexpr int RowsAtCompileTime = TargetType::RowsAtCompileTime;
    constexpr int ColsAtCompileTime = TargetType::ColsAtCompileTime;
    constexpr int row_major = TargetType::IsRowMajor;

    if (RowsAtCompileTime > 0 && this->rows() != RowsAtCompileTime) return false;
    if (ColsAtCompileTime > 0 && this->cols() != ColsAtCompileTime) return false;
    if (ScalarToEnum_v<Scalar> != m_scalar_type) return false;
    if (!ignore_storage_order && this->rows() != 1 && this->cols() != 1 &&
        row_major != is_row_major()) {
        lagrange::logger().error(
            "Target storage order ({}) does not match array storage order ({}).",
            row_major == 1 ? "RowMajor" : "ColMajor",
            is_row_major() == 1 ? "RowMajor" : "ColMajor");
        return false;
    }

    return true;
}


template <typename Derived>
std::unique_ptr<ArrayBase> ArrayBase::row_slice_impl(
    const Eigen::MatrixBase<Derived>& matrix,
    Index num_rows,
    const IndexFunction& mapping_fn)
{
    using Scalar = typename Derived::Scalar;
    using OutEigenType = Eigen::Matrix<
        Scalar,
        Eigen::Dynamic,
        Derived::ColsAtCompileTime,
        Derived::IsRowMajor ? Eigen::RowMajor : Eigen::ColMajor>;

    const auto num_cols = matrix.cols();
    OutEigenType out_matrix(num_rows, num_cols);
    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, num_rows),
        [&](const tbb::blocked_range<size_t>& r) {
            for (auto i = r.begin(); i != r.end(); i++) {
                out_matrix.row(i) = matrix.row(mapping_fn(i));
            }
        });

    return std::make_unique<EigenArray<OutEigenType>>(std::move(out_matrix));
}

template <typename Derived>
std::enable_if_t<std::is_integral<typename Derived::Scalar>::value, std::unique_ptr<ArrayBase>>
ArrayBase::row_slice_impl(
    const Eigen::MatrixBase<Derived>& matrix,
    Index num_rows,
    const WeightedIndexFunction& mapping_fn)
{
    using Scalar = typename Derived::Scalar;
    using OutEigenType = Eigen::Matrix<
        Scalar,
        Eigen::Dynamic,
        Derived::ColsAtCompileTime,
        Derived::IsRowMajor ? Eigen::RowMajor : Eigen::ColMajor>;
    const auto num_cols = matrix.cols();
    tbb::enumerable_thread_specific<std::vector<std::pair<Index, double>>> weights;

    using DoubleEigenType = Eigen::Matrix<
        double,
        Eigen::Dynamic,
        Derived::ColsAtCompileTime,
        Derived::IsRowMajor ? Eigen::RowMajor : Eigen::ColMajor>;
    DoubleEigenType out_matrix(num_rows, num_cols);
    out_matrix.setZero();

    tbb::parallel_for(
        tbb::blocked_range<Index>(0, num_rows),
        [&](const tbb::blocked_range<Index>& r) {
            for (auto i = r.begin(); i != r.end(); i++) {
                auto& entries = weights.local();
                mapping_fn(i, entries);
                for (const auto& entry : entries) {
                    out_matrix.row(i) +=
                        matrix.row(entry.first).template cast<double>() * entry.second;
                }
            }
        });
    return std::make_unique<EigenArray<OutEigenType>>(
        std::move(out_matrix.array().round().template cast<Scalar>().matrix().eval()));
}

template <typename Derived>
std::enable_if_t<!std::is_integral<typename Derived::Scalar>::value, std::unique_ptr<ArrayBase>>
ArrayBase::row_slice_impl(
    const Eigen::MatrixBase<Derived>& matrix,
    Index num_rows,
    const WeightedIndexFunction& mapping_fn)
{
    using Scalar = typename Derived::Scalar;
    using OutEigenType = Eigen::Matrix<
        Scalar,
        Eigen::Dynamic,
        Derived::ColsAtCompileTime,
        Derived::IsRowMajor ? Eigen::RowMajor : Eigen::ColMajor>;
    const auto num_cols = matrix.cols();
    tbb::enumerable_thread_specific<std::vector<std::pair<Index, double>>> weights;

    OutEigenType out_matrix(num_rows, num_cols);
    out_matrix.setZero();

    tbb::parallel_for(
        tbb::blocked_range<Index>(0, num_rows),
        [&](const tbb::blocked_range<Index>& r) {
            for (auto i = r.begin(); i != r.end(); i++) {
                auto& entries = weights.local();
                mapping_fn(i, entries);
                for (const auto& entry : entries) {
                    out_matrix.row(i) += matrix.row(entry.first).template cast<Scalar>() *
                                         safe_cast<Scalar>(entry.second);
                }
            }
        });
    return std::make_unique<EigenArray<OutEigenType>>(std::move(out_matrix));
}

} // namespace experimental
} // namespace lagrange

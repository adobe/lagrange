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

#include <algorithm>
#include <exception>
#include <iostream>

#include <lagrange/Logger.h>
#include <lagrange/common.h>
#include <lagrange/experimental/Scalar.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>
#include <lagrange/utils/strings.h>

namespace lagrange {
namespace experimental {

class ArrayBase
{
public:
    using Index = Eigen::Index; // Default to std::ptrdiff_t

public:
    ArrayBase(ScalarEnum type)
        : m_scalar_type(type)
    {}
    virtual ~ArrayBase() = default;

public:
    template <typename TargetType>
    Eigen::Map<TargetType> view()
    {
        la_runtime_assert(is_compatible<TargetType>(), "Target view type is not compatible with the data.");
        return Eigen::Map<TargetType>(data<typename TargetType::Scalar>(), rows(), cols());
    }

    template <typename TargetType>
    Eigen::Map<const TargetType> view() const
    {
        la_runtime_assert(is_compatible<TargetType>(), "Target view type is not compatible with the data.");
        return Eigen::Map<const TargetType>(data<typename TargetType::Scalar>(), rows(), cols());
    }

    template <typename Derived>
    void set(const Eigen::MatrixBase<Derived>& data);

    template <typename Derived>
    void set(Eigen::MatrixBase<Derived>&& data);

    ScalarEnum get_scalar_type() const { return m_scalar_type; }

    template <typename TargetType>
    const auto& get() const;

    template <typename TargetType>
    auto& get();

    template <typename Scalar>
    Scalar* data()
    {
        la_runtime_assert(ScalarToEnum_v<Scalar> == m_scalar_type);
        return static_cast<Scalar*>(data());
    }

    template <typename Scalar>
    const Scalar* data() const
    {
        la_runtime_assert(ScalarToEnum_v<Scalar> == m_scalar_type);
        return static_cast<const Scalar*>(data());
    }

    virtual Index rows() const = 0;
    virtual Index cols() const = 0;
    virtual bool is_row_major() const = 0;
    virtual void* data() = 0;
    virtual const void* data() const = 0;
    virtual void resize(Index, Index) = 0;
    virtual std::unique_ptr<ArrayBase> clone() const = 0;

    using IndexFunction = std::function<Index(Index)>;
    using WeightedIndexFunction =
        std::function<void(Index, std::vector<std::pair<Index, double>>&)>;

    template <typename T>
    std::unique_ptr<ArrayBase> row_slice(const std::vector<T>& row_indices) const
    {
        return row_slice(safe_cast<Index>(row_indices.size()), [&](Index i) {
            return safe_cast<Index>(row_indices[i]);
        });
    }

    /**
     * Using index function for row mapping.
     *
     * @param mapping_fn: An index mapping function:
     *                    input_row_index = mapping_fn(output_row_index)).
     */
    virtual std::unique_ptr<ArrayBase> row_slice(Index num_rows, const IndexFunction& mapping_fn)
        const = 0;

    /**
     * This is the most generic version of row_slice method.
     *
     * @param mapping_fn: A mapping function that maps each output row index to
     *                    a vector of (input_row_index, weight).
     */
    virtual std::unique_ptr<ArrayBase> row_slice(
        Index num_rows,
        const WeightedIndexFunction& mapping_fn) const = 0;

    virtual std::string type_name() const = 0;

protected:
    template <typename TargetType>
    bool is_compatible(bool ignore_storage_order = false) const;

    template <typename Derived>
    static std::unique_ptr<ArrayBase> row_slice_impl(
        const Eigen::MatrixBase<Derived>& matrix,
        Index num_rows,
        const IndexFunction& mapping);

    template <typename Derived>
    static std::
        enable_if_t<std::is_integral<typename Derived::Scalar>::value, std::unique_ptr<ArrayBase>>
        row_slice_impl(
            const Eigen::MatrixBase<Derived>& matrix,
            Index num_rows,
            const WeightedIndexFunction& mapping);

    template <typename Derived>
    static std::
        enable_if_t<!std::is_integral<typename Derived::Scalar>::value, std::unique_ptr<ArrayBase>>
        row_slice_impl(
            const Eigen::MatrixBase<Derived>& matrix,
            Index num_rows,
            const WeightedIndexFunction& mapping);

protected:
    const ScalarEnum m_scalar_type;
};


/**
 * This class is a thin wrapper around an Eigen matrix.  It takes ownership of
 * the Eigen matrix.
 */
template <typename _EigenType>
class EigenArray : public ArrayBase
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    using EigenType = _EigenType;
    using Index = ArrayBase::Index;
    using ArrayBase::WeightedIndexFunction;
    static_assert(
        std::is_base_of<typename Eigen::EigenBase<EigenType>, EigenType>::value,
        "Template parameter `_EigenType` is not an Eigen type!");
    static_assert(!std::is_const<EigenType>::value, "EigenType should not be const type.");
    static_assert(!std::is_reference<EigenType>::value, "EigenType should not be reference type.");

public:
    EigenArray()
        : ArrayBase(ScalarToEnum_v<typename EigenType::Scalar>)
    {}

    template <typename T>
    explicit EigenArray(const Eigen::MatrixBase<T>& data)
        : ArrayBase(ScalarToEnum_v<typename EigenType::Scalar>)
        , m_data(data)
    {}

    template <typename T>
    explicit EigenArray(Eigen::MatrixBase<T>&& data)
        : ArrayBase(ScalarToEnum_v<typename EigenType::Scalar>)
        , m_data(std::move(data.derived()))
    {}

public:
    EigenType& get_ref() { return m_data; }
    const EigenType& get_ref() const { return m_data; }

    template <typename Derived>
    void set(Derived&& data)
    {
        m_data = std::forward<Derived>(data);
    }

    Index rows() const override { return m_data.rows(); }
    Index cols() const override { return m_data.cols(); }
    bool is_row_major() const override { return EigenType::IsRowMajor != 0; }
    void* data() override { return static_cast<void*>(m_data.derived().data()); }
    const void* data() const override { return static_cast<const void*>(m_data.derived().data()); }
    void resize(Index r, Index c) override { m_data.resize(r, c); }

    std::unique_ptr<ArrayBase> clone() const override
    {
        return std::make_unique<EigenArray<EigenType>>(m_data);
    }

    std::unique_ptr<ArrayBase> row_slice(Index num_rows, const IndexFunction& mapping_fn)
        const override
    {
        return row_slice_impl(m_data, num_rows, mapping_fn);
    }
    std::unique_ptr<ArrayBase> row_slice(Index num_rows, const WeightedIndexFunction& mapping_fn)
        const override
    {
        return row_slice_impl(m_data, num_rows, mapping_fn);
    }

    std::string type_name() const override
    {
        std::string scalar_name(ScalarToEnum<typename EigenType::Scalar>::name);
        return string_format(
            "EigenArray<Eigen::Matrix<{}, {}, {}, {}>>",
            scalar_name,
            EigenType::RowsAtCompileTime,
            EigenType::ColsAtCompileTime,
            EigenType::Options);
    }

private:
    EigenType m_data;
};


/**
 * This class is a thin wrapper around an Eigen matrix.  Unlike EigenArray, this
 * class does __not__ take ownership of the Eigen matrix.
 */
template <typename _EigenType, bool IsConst = std::is_const<_EigenType>::value>
class EigenArrayRef;

template <typename _EigenType>
class EigenArrayRef<_EigenType, false> : public ArrayBase
{
public:
    using EigenType = _EigenType;
    using DecayedEigenType = std::decay_t<_EigenType>;
    using Index = ArrayBase::Index;
    using ArrayBase::WeightedIndexFunction;
    static_assert(
        std::is_base_of<typename Eigen::EigenBase<DecayedEigenType>, DecayedEigenType>::value,
        "Template parameter `_EigenType` is not an Eigen type!");
    static_assert(!std::is_const<EigenType>::value, "EigenType should not be const type.");
    static_assert(!std::is_reference<EigenType>::value, "EigenType should not be reference type.");

public:
    EigenArrayRef()
        : ArrayBase(ScalarToEnum_v<typename EigenType::Scalar>)
    {}

    template <typename T>
    explicit EigenArrayRef(Eigen::MatrixBase<T>& data)
        : ArrayBase(ScalarToEnum_v<typename EigenType::Scalar>)
        , m_data(data.derived())
    {}

public:
    EigenType& get_ref() { return m_data; }
    const EigenType& get_ref() const { return m_data; }

    template <typename Derived>
    void set(Derived&& data)
    {
        m_data = std::forward<Derived>(data);
    }

    Index rows() const override { return m_data.rows(); }
    Index cols() const override { return m_data.cols(); }
    bool is_row_major() const override { return EigenType::IsRowMajor != 0; }
    void* data() override { return static_cast<void*>(m_data.derived().data()); }
    const void* data() const override { return static_cast<const void*>(m_data.derived().data()); }
    void resize(Index r, Index c) override { m_data.resize(r, c); }

    std::unique_ptr<ArrayBase> clone() const override
    {
        return std::make_unique<EigenArray<DecayedEigenType>>(m_data);
    }

    std::unique_ptr<ArrayBase> row_slice(Index num_rows, const IndexFunction& mapping_fn)
        const override
    {
        return row_slice_impl(m_data, num_rows, mapping_fn);
    }
    std::unique_ptr<ArrayBase> row_slice(Index num_rows, const WeightedIndexFunction& mapping_fn)
        const override
    {
        return row_slice_impl(m_data, num_rows, mapping_fn);
    }

    std::string type_name() const override
    {
        std::string scalar_name(ScalarToEnum<typename EigenType::Scalar>::name);
        return string_format(
            "EigenArrayRef<Eigen::Matrix<{}, {}, {}, {}>>",
            scalar_name,
            EigenType::RowsAtCompileTime,
            EigenType::ColsAtCompileTime,
            EigenType::Options);
    }

private:
    EigenType& m_data;
};

template <typename _EigenType>
class EigenArrayRef<_EigenType, true> : public ArrayBase
{
public:
    using EigenType = _EigenType;
    using DecayedEigenType = std::decay_t<_EigenType>;
    using Index = ArrayBase::Index;
    using ArrayBase::WeightedIndexFunction;
    static_assert(
        std::is_base_of<typename Eigen::EigenBase<DecayedEigenType>, DecayedEigenType>::value,
        "Template parameter `_EigenType` is not an Eigen type!");
    static_assert(std::is_const<EigenType>::value, "EigenType must const type.");
    static_assert(!std::is_reference<EigenType>::value, "EigenType should not be reference type.");

public:
    template <typename T>
    explicit EigenArrayRef(const Eigen::MatrixBase<T>& data)
        : ArrayBase(ScalarToEnum_v<typename EigenType::Scalar>)
        , m_data(data.derived())
    {}

public:
    const EigenType& get_ref() const { return m_data; }

    Index rows() const override { return m_data.rows(); }
    Index cols() const override { return m_data.cols(); }
    bool is_row_major() const override { return EigenType::IsRowMajor != 0; }
    const void* data() const override { return static_cast<const void*>(m_data.derived().data()); }
    void* data() override { throw std::runtime_error("This method is not supported"); }

    void resize(Index r, Index c) override
    {
        if (r != m_data.rows() || c != m_data.cols()) {
            throw std::runtime_error("Resizing const EigenArrayRef is not allowed.");
        }
    }

    std::unique_ptr<ArrayBase> clone() const override
    {
        return std::make_unique<EigenArray<DecayedEigenType>>(m_data);
    }

    std::unique_ptr<ArrayBase> row_slice(Index num_rows, const IndexFunction& mapping_fn)
        const override
    {
        return row_slice_impl(m_data, num_rows, mapping_fn);
    }
    std::unique_ptr<ArrayBase> row_slice(Index num_rows, const WeightedIndexFunction& mapping_fn)
        const override
    {
        return row_slice_impl(m_data, num_rows, mapping_fn);
    }

    std::string type_name() const override
    {
        std::string scalar_name(ScalarToEnum<typename EigenType::Scalar>::name);
        return string_format(
            "EigenArrayRef<const Eigen::Matrix<{}, {}, {}, {}>>",
            scalar_name,
            EigenType::RowsAtCompileTime,
            EigenType::ColsAtCompileTime,
            EigenType::Options);
    }

private:
    EigenType& m_data;
};


/**
 * This class provide a thin wrapper around a raw array.  It does not assume
 * memory ownership of the memory.
 */
template <
    typename _Scalar,
    int _Rows = Eigen::Dynamic,
    int _Cols = Eigen::Dynamic,
    int _Options = Eigen::RowMajor,
    bool IsConst = std::is_const<_Scalar>::value>
class RawArray;

template <typename _Scalar, int _Rows, int _Cols, int _Options>
class RawArray<_Scalar, _Rows, _Cols, _Options, false> : public ArrayBase
{
public:
    using Scalar = _Scalar;
    using Index = ArrayBase::Index;
    using EigenType = Eigen::Matrix<Scalar, _Rows, _Cols, _Options>;
    using EigenMap = Eigen::Map<EigenType>;
    using ConstEigenMap = Eigen::Map<const EigenType>;
    using ArrayBase::WeightedIndexFunction;

public:
    RawArray(Scalar* data, Index rows, Index cols)
        : ArrayBase(ScalarToEnum_v<typename EigenType::Scalar>)
        , m_data(data, rows, cols)
    {}

public:
    EigenMap& get_ref() { return m_data; }
    ConstEigenMap get_ref() const { return m_data; }

    template <typename Derived>
    void set(const Eigen::MatrixBase<Derived>& data)
    {
        m_data = data;
    }

    void set(Scalar* data, Index rows, Index cols) { m_data = EigenMap(data, rows, cols); }

    Index rows() const override { return m_data.rows(); }
    Index cols() const override { return m_data.cols(); }
    bool is_row_major() const override { return EigenType::IsRowMajor != 0; }
    void* data() override { return static_cast<void*>(m_data.data()); }
    const void* data() const override { return static_cast<const void*>(m_data.data()); }
    void resize(Index r, Index c) override
    {
        if (r != m_data.rows() || c != m_data.cols()) {
            throw std::runtime_error("Resizing RawArray is not allowed.");
        }
    }

    std::unique_ptr<ArrayBase> clone() const override
    {
        return std::make_unique<EigenArray<EigenType>>(m_data);
    }

    std::unique_ptr<ArrayBase> row_slice(Index num_rows, const IndexFunction& mapping_fn)
        const override
    {
        return row_slice_impl(m_data, num_rows, mapping_fn);
    }
    std::unique_ptr<ArrayBase> row_slice(Index num_rows, const WeightedIndexFunction& mapping_fn)
        const override
    {
        return row_slice_impl(m_data, num_rows, mapping_fn);
    }

    std::string type_name() const override
    {
        std::string scalar_name(ScalarToEnum<Scalar>::name);
        return string_format("RawArray<{}, {}, {}, {}>", scalar_name, _Rows, _Cols, _Options);
    }

private:
    EigenMap m_data;
};

template <typename _Scalar, int _Rows, int _Cols, int _Options>
class RawArray<_Scalar, _Rows, _Cols, _Options, true> : public ArrayBase
{
public:
    using Scalar = std::decay_t<_Scalar>;
    using Index = ArrayBase::Index;
    using EigenType = Eigen::Matrix<Scalar, _Rows, _Cols, _Options>;
    using EigenMap = Eigen::Map<const EigenType>;
    using ArrayBase::WeightedIndexFunction;

public:
    RawArray(const Scalar* data, Index rows, Index cols)
        : ArrayBase(ScalarToEnum_v<typename EigenType::Scalar>)
        , m_data(data, rows, cols)
    {}

public:
    const EigenMap& get_ref() const { return m_data; }

    Index rows() const override { return m_data.rows(); }
    Index cols() const override { return m_data.cols(); }
    bool is_row_major() const override { return EigenType::IsRowMajor != 0; }
    const void* data() const override { return static_cast<const void*>(m_data.data()); }
    void* data() override { throw std::runtime_error("This method is not supported"); }

    void resize(Index r, Index c) override
    {
        if (r != m_data.rows() || c != m_data.cols()) {
            throw std::runtime_error("Resizing RawArray is not allowed.");
        }
    }

    std::unique_ptr<ArrayBase> clone() const override
    {
        return std::make_unique<EigenArray<EigenType>>(m_data);
    }

    std::unique_ptr<ArrayBase> row_slice(Index num_rows, const IndexFunction& mapping_fn)
        const override
    {
        return row_slice_impl(m_data, num_rows, mapping_fn);
    }
    std::unique_ptr<ArrayBase> row_slice(Index num_rows, const WeightedIndexFunction& mapping_fn)
        const override
    {
        return row_slice_impl(m_data, num_rows, mapping_fn);
    }

    std::string type_name() const override
    {
        std::string scalar_name(ScalarToEnum<Scalar>::name);
        return string_format("RawArray<const {}, {}, {}, {}>", scalar_name, _Rows, _Cols, _Options);
    }

private:
    const EigenMap m_data;
};

} // namespace experimental
} // namespace lagrange

#include "Array.impl.h"
#include "Array.serialization.h"

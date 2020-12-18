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
#include <Eigen/Core>

#include <lagrange/common.h>
#include <lagrange/utils/safe_cast.h>

/*
This file defines two iterable objects (Range and SparseRange),
and functions to create them.

Important: in the range_* functions, the `active` vector MUST
exist while the range is being used. Do NOT write code such as:

for (int i : range_sparse(n, { 0 })) { ... }   // Bad

as the vector will be destructed while the iterator is still running.

Similarily, do NOT write:

for (int i : range_facets(mesh, {})) { ... }    // Bad

But use this instead:

for (int i : range_facets(mesh)) { ... }
*/

namespace lagrange {

namespace internal {
// Forward declaration of Range objects later in the file.
template <typename T>
struct IteratorTrait;
template <typename T, typename Trait = IteratorTrait<T>>
class Range;
template <typename T, typename Trait = IteratorTrait<T>>
class SparseRange;
template <typename Derived, typename Trait = IteratorTrait<Derived>>
class RowRange;
} // namespace internal

// Returns an iterable object, ranging from 0 to end.
template <typename T>
internal::Range<T> range(T end)
{
    return internal::Range<T>(end);
}

// Returns an iterable object, ranging from begin to end.
template <typename T>
internal::Range<T> range(T begin, T end)
{
    return internal::Range<T>(begin, end);
}

// Returns an iterable object ranging through the rows of data.
template <typename Derived>
internal::RowRange<Derived> row_range(const Derived& data)
{
    return internal::RowRange<Derived>(data);
}

// Returns an iterable object.
// If `active` is non-empty, it will iterate through the elements of `active`.
// Otherwise, it will iterate from 0 to max.
template <typename T>
internal::SparseRange<T> range_sparse(T max, const std::vector<T>& active)
{
    return internal::SparseRange<T>(max, active);
}

// Returns an iterable object, ranging from 0 to mesh->num_facets.
template <typename MeshType>
internal::Range<typename MeshType::Index> range_facets(const MeshType& mesh)
{
    return internal::Range<typename MeshType::Index>(mesh.get_num_facets());
}

// Returns an iterable object.
// If `active` is non-empty, it will iterate through the elements of `active`.
// Otherwise, it will iterate from 0 to mesh->num_facets.
template <typename MeshType>
internal::SparseRange<typename MeshType::Index> range_facets(
    const MeshType& mesh,
    const typename MeshType::IndexList& active)
{
    return internal::SparseRange<typename MeshType::Index>(mesh.get_num_facets(), active);
}

// Returns an iterable object, ranging from 0 to mesh->num_vertices.
template <typename MeshType>
internal::Range<typename MeshType::Index> range_vertices(const MeshType& mesh)
{
    return internal::Range<typename MeshType::Index>(mesh.get_num_vertices());
}

// Returns an iterable object.
// If `active` is non-empty, it will iterate through the elements of `active`.
// Otherwise, it will iterate from 0 to mesh->num_vertices.
template <typename MeshType>
internal::SparseRange<typename MeshType::Index> range_vertices(
    const MeshType& mesh,
    const typename MeshType::IndexList& active)
{
    return internal::SparseRange<typename MeshType::Index>(mesh.get_num_vertices(), active);
}

// Prevent bad calls
template <typename T>
internal::SparseRange<T> range_sparse(T max, std::vector<T>&& active)
{
    static_assert(StaticAssertableBool<T>::False, "active indices cannot be an rvalue vector");
}

template <typename MeshType>
internal::SparseRange<typename MeshType::Index> range_facets(
    const MeshType& mesh,
    typename MeshType::IndexList&& active)
{
    static_assert(
        StaticAssertableBool<MeshType>::False,
        "active indices cannot be an rvalue vector");
}

template <typename MeshType>
internal::SparseRange<typename MeshType::Index> range_vertices(
    const MeshType& mesh,
    typename MeshType::IndexList&& active)
{
    static_assert(
        StaticAssertableBool<MeshType>::False,
        "active indices cannot be an rvalue vector");
}


namespace internal {

template <typename T>
struct IteratorTrait
{
    using iterator_category = std::input_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t; // Not used.
    using pointer = T*;
    using reference = T&;
};

template <typename T, typename Trait>
class Range
{
public:
    Range(T end)
        : m_begin(0)
        , m_end(end)
    {}
    Range(T begin, T end)
        : m_begin(begin)
        , m_end(end)
    {}

    class iterator
    {
    public:
        using iterator_category = typename Trait::iterator_category;
        using value_type = typename Trait::value_type;
        using difference_type = typename Trait::difference_type;
        using pointer = typename Trait::pointer;
        using reference = typename Trait::reference;

    public:
        explicit iterator(T val, const Range<T>& c)
            : m_value(val)
            , m_context(c)
        {}

        const T& operator*() const { return m_value; }

        iterator& operator++()
        {
            ++m_value;
            return *this;
        }

        bool operator!=(const iterator other) const
        {
            if (m_context == other.m_context) {
                return m_value < other.m_value;
            } else {
                return true;
            }
        }

    private:
        T m_value;
        const Range<T>& m_context;
    };

    iterator begin() const { return iterator(m_begin, *this); }

    iterator end() const { return iterator(m_end, *this); }

    bool operator==(const Range<T>& other) const
    {
        return m_begin == other.m_begin && m_end == other.m_end;
    }

private:
    T m_begin;
    T m_end;
};

template <typename T, typename Trait>
class SparseRange
{
    const T m_max;
    const std::vector<T>& m_active;

public:
    SparseRange(T max, const std::vector<T>& active)
        : m_max(max)
        , m_active(active)
    {}

    class iterator
    {
    public:
        using iterator_category = typename Trait::iterator_category;
        using value_type = typename Trait::value_type;
        using difference_type = typename Trait::difference_type;
        using pointer = typename Trait::pointer;
        using reference = typename Trait::reference;

    public:
        iterator(T val, const SparseRange& context)
            : m_context(context)
            , i(val)
        {}

        bool operator!=(const iterator& rhs) const
        {
            return (m_context != rhs.m_context) || (i != rhs.i);
        }

        iterator& operator++()
        {
            ++i;
            return *this;
        }

        T operator*() const
        {
            if (m_context.m_active.empty()) return i;
            return m_context.m_active[i];
        }

    private:
        const SparseRange& m_context;
        T i;
    };

    iterator begin() const { return iterator(0, *this); }
    iterator end() const
    {
        return iterator(m_active.empty() ? m_max : safe_cast<T>(m_active.size()), *this);
    }

    bool operator!=(const SparseRange<T> other) const
    {
        return m_max != other.m_max || m_active.size() != other.m_active.size();
    }
};


template <typename Derived, typename Trait>
class RowRange
{
private:
    const Derived& m_data;

public:
    using Index = Eigen::Index;
    using RowExpr = decltype(m_data.row(0));
    RowRange(const Derived& data)
        : m_data(data)
    {}

    class iterator
    {
    public:
        using iterator_category = typename Trait::iterator_category;
        using value_type = typename Trait::value_type;
        using difference_type = typename Trait::difference_type;
        using pointer = typename Trait::pointer;
        using reference = typename Trait::reference;

    public:
        iterator(Index row_index, const Derived& context)
            : m_row_index(row_index)
            , m_context(context)
        {}

        decltype(auto) operator*() const { return m_context.row(m_row_index); }

        iterator& operator++()
        {
            m_row_index++;
            return *this;
        }

        bool operator!=(const iterator& other) const
        {
            // We compare the address of the context since comparing
            // the actual Eigen matrix will be too expensive.
            return &m_context != &other.m_context || m_row_index != other.m_row_index;
        }

        bool operator==(const iterator& other) const { return !(*this != other); }

    private:
        Index m_row_index;
        const Derived& m_context;
    };

    iterator begin() const { return iterator(0, m_data); }

    iterator end() const { return iterator(m_data.rows(), m_data); }

    bool operator==(const RowRange<Derived>& other) const
    {
        // Checking address only as checking the entire matrix will be
        // too expensive.
        return &m_data == &other.m_data;
    }

    Index size() const { return m_data.rows(); }
};

} // namespace internal


} // namespace lagrange

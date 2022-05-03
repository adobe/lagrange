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

#include <lagrange/utils/safe_cast.h>

/// @addtogroup group-utils-misc
/// @{

namespace lagrange {

////////////////////////////////////////////////////////////////////////////////
/// @cond LA_INTERNAL_DOCS

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

template <typename T, typename Trait = IteratorTrait<T>>
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

template <typename T, typename Trait = IteratorTrait<T>>
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

} // namespace internal

/// @endcond
////////////////////////////////////////////////////////////////////////////////

///
/// Returns an iterable object representing the range [0, end).
///
/// @param[in]  end    End of the range. This bound is exclusive.
///
/// @tparam     Index  Index type.
///
/// @return     Iterator for the range [0, end).
///
template <typename Index>
internal::Range<Index> range(Index end)
{
    return internal::Range<Index>(end);
}

///
/// Returns an iterable object representing the range [begin, end).
///
/// @param[in]  begin  Start of the range. This bound is inclusive.
/// @param[in]  end    End of the range. This bound is exclusive.
///
/// @tparam     Index  Index type.
///
/// @return     Iterator for the range [start, end).
///
template <typename Index>
internal::Range<Index> range(Index begin, Index end)
{
    return internal::Range<Index>(begin, end);
}

///
/// Returns an iterable object representing a subset of the range [0, max). If active is non-empty,
/// it will iterate through the elements of active. Otherwise, it will iterate from 0 to max.
///
/// @warning    The `active` vector MUST exist while the range is being used. Do NOT write code such
///             as:
///             @code
///             for (int i : range_sparse(n, { 0 })) { ... }   // Bad
///             @endcode
///             as the vector will be destructed while the iterator is still running.
///
/// @param[in]  max     End of the range. This bound is exclusive.
/// @param[in]  active  Active indices in the range. If empty, it will iterate through the whole
///                     range instead.
///
/// @tparam     Index  Index type.
///
/// @return     Sparse iterator for a subset of the range [0, max).
///
template <typename Index>
internal::SparseRange<Index> range_sparse(Index max, const std::vector<Index>& active)
{
    return internal::SparseRange<Index>(max, active);
}

///
/// @cond LA_INTERNAL_DOCS
///
/// Prevent bad calls.
///
template <typename T>
internal::SparseRange<T> range_sparse(T /*max*/, std::vector<T>&& /*active*/) = delete;
/// @endcond

} // namespace lagrange

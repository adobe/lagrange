// Source: https://github.com/X-czh/smart_ptr
// SPDX-License-Identifier: MIT
//
// This file has been modified by Adobe.
//
// All modifications are Copyright 2022 Adobe.

// default_delete is the default destruction policy used by
// unique_ptr & shared_ptr when no deleter is specified.

#pragma once

namespace lagrange::internal {

// 20.7.1.1 Default deleters

// 20.7.1.1.2, default_delete

template <typename T>
class default_delete
{
public:
    /// Default constructor
    constexpr default_delete() noexcept = default;

    /// Converting constructor, convertibility is not checked
    template <typename U>
    default_delete(const default_delete<U>&) noexcept
    {}

    /// Call operator
    void operator()(T* p) const { delete p; }
};

// 20.7.1.1.3, default_delete<T[]>

template <typename T>
class default_delete<T[]>
{
public:
    /// Default constructor
    constexpr default_delete() noexcept = default;

    /// Converting constructor, convertibility is not checked
    template <typename U>
    default_delete(const default_delete<U[]>&) noexcept
    {}

    /// Call operator
    void operator()(T* p) const { delete[] p; }
};

} // namespace lagrange::internal

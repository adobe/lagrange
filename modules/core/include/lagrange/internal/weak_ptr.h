// Source: https://github.com/X-czh/smart_ptr
// SPDX-License-Identifier: MIT
//
// This file has been modified by Adobe.
//
// All modifications are Copyright 2022 Adobe.

#pragma once

#include <lagrange/internal/shared_ptr.h>
#include <lagrange/internal/smart_ptr/control_block.h>

#include <type_traits> // remove_extent

namespace lagrange::internal {

// Forward declarations

template <typename T>
class shared_ptr;

// 20.7.2.3 Class template weak_ptr

template <typename T>
class weak_ptr
{
public:
    template <typename U>
    friend class shared_ptr;

    template <typename U>
    friend class weak_ptr;

    using element_type = typename std::remove_extent<T>::type;

    // 20.7.2.3.1, constructors:

    /// Default constructor, creates an empty weak_ptr
    /// Postconditions: use_count() == 0.
    constexpr weak_ptr() noexcept
        : _ptr{}
        , _control_block{}
    {}

    /// Conversion constructor: shares ownership with sp
    /// Postconditions: use_count() == sp.use_count().
    template <class U>
    weak_ptr(shared_ptr<U> const& sp) noexcept
        : _ptr{sp._ptr}
        , _control_block{sp._control_block}
    {
        if (_control_block) _control_block->inc_wref();
    }

    /// Copy constructor: shares ownership with wp
    /// Postconditions: use_count() == wp.use_count().
    weak_ptr(weak_ptr const& wp) noexcept
        : _ptr{wp._ptr}
        , _control_block{wp._control_block}
    {
        if (_control_block) _control_block->inc_wref();
    }

    /// Copy constructor: shares ownership with wp
    /// Postconditions: use_count() == wp.use_count().
    template <class U>
    weak_ptr(weak_ptr<U> const& wp) noexcept
        : _ptr{wp._ptr}
        , _control_block{wp._control_block}
    {
        if (_control_block) _control_block->inc_wref();
    }

    // 20.7.2.3.2, destructor

    ~weak_ptr()
    {
        if (_control_block) _control_block->dec_wref();
    }

    // 20.7.2.3.3, assignment

    weak_ptr& operator=(const weak_ptr& wp) noexcept
    {
        weak_ptr{wp}.swap(*this);
        return *this;
    }

    template <typename U>
    weak_ptr& operator=(const weak_ptr<U>& wp) noexcept
    {
        weak_ptr{wp}.swap(*this);
        return *this;
    }

    template <typename U>
    weak_ptr& operator=(const shared_ptr<U>& sp) noexcept
    {
        weak_ptr{sp}.swap(*this);
        return *this;
    }

    // 20.7.2.3.4, modifiers

    /// Exchanges the contents of *this and sp
    void swap(weak_ptr& wp) noexcept
    {
        using std::swap;
        swap(_ptr, wp._ptr);
        swap(_control_block, wp._control_block);
    }

    /// Resets *this to empty
    void reset() noexcept { weak_ptr{}.swap(*this); }

    // 20.7.2.3.5, observers

    /// Gets use_count
    long use_count() const noexcept { return (_control_block) ? _control_block->use_count() : 0; }

    /// Checks if use_count == 0
    bool expired() const noexcept { return (_control_block) ? _control_block->expired() : false; }

    /// Checks if there is a managed object
    shared_ptr<T> lock() const noexcept
    {
        return (expired()) ? shared_ptr<T>{} : shared_ptr<T>{*this};
    }

private:
    element_type* _ptr;
    control_block_base* _control_block;
};

// 20.7.2.3.6, specialized algorithm

/// Swaps with another weak_ptr
template <typename T>
inline void swap(weak_ptr<T>& wp1, weak_ptr<T>& wp2)
{
    wp1.swap(wp2);
}

} // namespace lagrange::internal

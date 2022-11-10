// Source: https://github.com/X-czh/smart_ptr
// SPDX-License-Identifier: MIT
//
// This file has been modified by Adobe.
//
// All modifications are Copyright 2022 Adobe.

#pragma once

#include <tuple> // tuple, get(tuple)

namespace lagrange::internal {

// ptr class that wraps the deleter, use tuple for Empty Base Optimization

template <typename T, typename D>
class ptr
{
public:
    using pointer = T*;
    using deleter_type = D;

    constexpr ptr() noexcept = default;

    ptr(pointer p)
        : _impl_t{}
    {
        _impl_ptr() = p;
    }

    template <typename Del>
    ptr(pointer p, Del&& d)
        : _impl_t{p, std::forward<Del>(d)}
    {}

    ~ptr() noexcept = default;

    pointer& _impl_ptr() { return std::get<0>(_impl_t); }
    pointer _impl_ptr() const { return std::get<0>(_impl_t); }
    deleter_type& _impl_deleter() { return std::get<1>(_impl_t); }
    const deleter_type& _impl_deleter() const { return std::get<1>(_impl_t); }

private:
    std::tuple<pointer, deleter_type> _impl_t;
};

} // namespace lagrange::internal

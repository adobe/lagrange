// Source: https://github.com/TartanLlama/function_ref
// SPDX-License-Identifier: CC0-1.0
//
// This file has been modified by Adobe.
//
// All modifications are Copyright 2022 Adobe.
//
// function_ref - A low-overhead non-owning function
// Written in 2017 by Simon Brand (@TartanLlama)
//
// To the extent possible under law, the author(s) have dedicated all
// copyright and related and neighboring rights to this software to the
// public domain worldwide. This software is distributed without any warranty.
//
// You should have received a copy of the CC0 Public Domain Dedication
// along with this software. If not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.
//

#pragma once

#include <functional>
#include <type_traits>
#include <utility>

namespace lagrange {

/// @defgroup group-utils-misc-functionref function_ref
/// @ingroup group-utils-misc
/// A lightweight non-owning reference to a callable.
/// @{

/// @ingroup group-utils-misc-functionref
///
/// A lightweight non-owning reference to a callable.
///
/// Example usage:
///
/// ```cpp
/// void foo (function_ref<int(int)> func) {
///     std::cout << "Result is " << func(21); //42
/// }
///
/// foo([](int i) { return i*2; });
/// ```
template <class F>
class function_ref;

/// Specialization for function types.
/// @ingroup group-utils-misc-functionref
template <class R, class... Args>
class function_ref<R(Args...)>
{
public:
    constexpr function_ref() noexcept = delete;

    /// Creates a `function_ref` which refers to the same callable as `rhs`.
    constexpr function_ref(const function_ref<R(Args...)>& rhs) noexcept = default;

    /// Constructs a `function_ref` referring to `f`.
    template <
        typename F,
        std::enable_if_t<
            !std::is_same<std::decay_t<F>, function_ref>::value &&
            std::is_invocable_r<R, F&&, Args...>::value>* = nullptr>
    constexpr function_ref(F&& f) noexcept
        : obj_(const_cast<void*>(reinterpret_cast<const void*>(std::addressof(f))))
    {
        callback_ = [](void* obj, Args... args) -> R {
            return std::invoke(
                *reinterpret_cast<typename std::add_pointer<F>::type>(obj),
                std::forward<Args>(args)...);
        };
    }

    /// Makes `*this` refer to the same callable as `rhs`.
    constexpr function_ref<R(Args...)>& operator=(const function_ref<R(Args...)>& rhs) noexcept =
        default;

    /// Makes `*this` refer to `f`.
    template <typename F, std::enable_if_t<std::is_invocable_r<R, F&&, Args...>::value>* = nullptr>
    constexpr function_ref<R(Args...)>& operator=(F&& f) noexcept
    {
        obj_ = reinterpret_cast<void*>(std::addressof(f));
        callback_ = [](void* obj, Args... args) {
            return std::invoke(
                *reinterpret_cast<typename std::add_pointer<F>::type>(obj),
                std::forward<Args>(args)...);
        };

        return *this;
    }

    /// Swaps the referred callables of `*this` and `rhs`.
    constexpr void swap(function_ref<R(Args...)>& rhs) noexcept
    {
        std::swap(obj_, rhs.obj_);
        std::swap(callback_, rhs.callback_);
    }

    /// Call the stored callable with the given arguments.
    R operator()(Args... args) const { return callback_(obj_, std::forward<Args>(args)...); }

    /// Checks whether *this stores a callable function target, i.e. is not empty.
    explicit operator bool() const noexcept { return callback_ != nullptr; }

private:
    void* obj_ = nullptr;
    R (*callback_)(void*, Args...) = nullptr;
};

/// Swaps the referred callables of `lhs` and `rhs`.
template <typename R, typename... Args>
constexpr void swap(function_ref<R(Args...)>& lhs, function_ref<R(Args...)>& rhs) noexcept
{
    lhs.swap(rhs);
}

/// Deduce function_ref type from a function pointer.
template <typename R, typename... Args>
function_ref(R (*)(Args...)) -> function_ref<R(Args...)>;

// TODO, will require some kind of callable traits
// template <typename F>
// function_ref(F) -> function_ref</* deduced if possible */>;

/// @}

} // namespace lagrange

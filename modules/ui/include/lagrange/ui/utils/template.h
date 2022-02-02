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
#include <lagrange/utils/assert.h>
#include <lagrange/utils/strings.h>
#include <lagrange/utils/warning.h>

#include <any>
#include <functional>


namespace lagrange {
namespace ui {


namespace util {

template <typename F>
struct lambda_helper;

template <typename F, typename Result, typename... Args>
struct lambda_helper<Result (F::*)(Args...) const>
{
    using fn_type = std::function<Result(Args...)>;
};
} // namespace util

namespace util {

template <class T>
struct AsFunction : public AsFunction<decltype(&T::operator())>
{
};

template <class Class, class ReturnType, class Arg>
struct AsFunction<ReturnType (Class::*)(Arg) const>
{
    using arg_type = Arg;
};

template <class ReturnType, class Arg>
struct AsFunction<ReturnType (*)(Arg)>
{
    using arg_type = Arg;
};

template <class ReturnType, class Arg>
struct AsFunction<ReturnType(Arg) const>
{
    using arg_type = Arg;
};

} // namespace util

namespace detail {


template <typename T>
struct convert_implicit
{
    using type = std::conditional_t<std::is_same<T, const char*>::value, std::string, T>;
};

template <typename T>
using convert_implicit_t = typename convert_implicit<T>::type;


template <typename... Args>
struct args_to_tuple
{
    using type = std::tuple<std::remove_cv_t<std::remove_reference_t<convert_implicit_t<Args>>>...>;
};

template <typename... Args>
using args_to_tuple_t = typename args_to_tuple<Args...>::type;


template <typename T>
std::unique_ptr<T> realize_default(
    typename std::enable_if<std::is_default_constructible<T>::value>::type* dummy = 0)
{
    LA_IGNORE(dummy);
    return std::make_unique<T>();
}

template <typename T>
std::unique_ptr<T> realize_default(
    typename std::enable_if<!std::is_default_constructible<T>::value>::type* dummy = 0)
{
    LA_IGNORE(dummy);

    la_runtime_assert(
        false,
        lagrange::string_format(
            "Cannot default construct type {}, provide a realization function to ResourceFactory",
            typeid(T).name()));
    return nullptr;
}


template <typename T, typename... Dummy>
std::shared_ptr<T> realize_forward(const std::shared_ptr<T>& ptr)
{
    return ptr;
}

template <typename T, typename... Args>
std::shared_ptr<T> realize_forward(
    Args... args,
    typename std::enable_if<std::is_constructible<T, Args...>::value>::type* dummy = 0)
{
    LA_IGNORE(dummy);
    return std::make_shared<T>(std::forward<Args>(args)...);
}

template <typename... Args>
void unused_variadic(Args&&... args)
{
    LA_IGNORE(sizeof...(args));
}


// Enabled if:
// 1. Cannot construct type T from Args
// 2. Cannot construct shared_ptr<T> from Args (makes sure this doesn't shadow realize_forward(const
// std::shared_ptr<T>&ptr))
// 3. Cannot construct unique_tr<T> from Args (makes sure this doesn't shadow
// realize_forward(std::unique_ptr<T>&&))
template <typename T, typename... Args>
std::shared_ptr<T> realize_forward(
    Args... args,
    typename std::enable_if<
        !std::is_constructible<T, Args...>::value &&
        !std::is_constructible<std::shared_ptr<T>, Args...>::value &&
        !std::is_constructible<std::unique_ptr<T>, Args...>::value>::type* dummy = 0)
{
    LA_IGNORE(dummy);
    unused_variadic(std::forward<Args>(args)...);
    la_runtime_assert(
        false,
        lagrange::string_format(
            "Cannot construct type {} from given arguments {}, provide a realization function to "
            "ResourceFactory",
            typeid(T).name(),
            typeid(std::tuple<Args...>).name()));

    return nullptr;
}

// Simply copies the data
template <typename T_out, typename T_in>
T_out copy_or_move_element(
    T_in& in,
    typename std::enable_if<!std::is_rvalue_reference<T_out>::value>::type* dummy = 0)
{
    LA_IGNORE(dummy);
    return in;
}

// Moves out the data
template <typename T_out, typename T_in>
std::remove_reference_t<T_out> copy_or_move_element(
    T_in& in,
    typename std::enable_if<std::is_rvalue_reference<T_out>::value>::type* dummy = 0)
{
    LA_IGNORE(dummy);
    std::remove_reference_t<T_out> moved = std::move(in);
    return moved;
}

/// Moves data if they are defined as rvalue in ArgumentTuple, otherwise copies
template <typename ArgumentTuple, typename... Ts, size_t... Is>
auto copy_or_move_impl(std::tuple<Ts...>& input, std::index_sequence<Is...>)
{
    return std::tuple<Ts...>{
        copy_or_move_element<std::tuple_element_t<Is, ArgumentTuple>>(std::get<Is>(input))...};
}

/// Moves data if they are defined as rvalue in ArgumentTuple, otherwise copies
template <typename ArgumentTuple, typename... Ts>
std::tuple<Ts...> copy_or_move(std::tuple<Ts...>& input)
{
    return copy_or_move_impl<ArgumentTuple>(input, std::make_index_sequence<sizeof...(Ts)>{});
}

template <typename ParamPack, typename ParamPackOriginal, typename F>
void apply_parameters(
    const F& fn,
    std::any& storage,
    typename std::enable_if<std::is_constructible<ParamPack, const ParamPack&>::value>::type*
        dummy = 0)
{
    LA_IGNORE(dummy);
    // If original param pack require rvalue reference, move the data, otherwise copy
    auto& storage_tuple = std::any_cast<ParamPack&>(storage);
    auto forwarded_tuple = copy_or_move<ParamPackOriginal>(storage_tuple);

    std::apply(fn, std::move(forwarded_tuple));
}


template <typename ParamPack, typename ParamPackOriginal, typename F>
void apply_parameters(
    const F& fn,
    std::any& storage,
    typename std::enable_if<!std::is_constructible<ParamPack, const ParamPack&>::value>::type*
        dummy = 0)
{
    // Do nothing
    LA_IGNORE(fn);
    LA_IGNORE(storage);
    LA_IGNORE(dummy);
    // Execution should not reach here as non-copyable arguments can't be saved to std::any
}


} // namespace detail

} // namespace ui
} // namespace lagrange

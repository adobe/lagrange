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


#include <lagrange/ui/BaseResourceData.h>
#include <lagrange/ui/FunctionUtils.h>
#include <lagrange/ui/ResourceUtils.h>
#include <lagrange/ui/ui_common.h>


#include <functional>
#include <memory>
#include <tuple>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

namespace lagrange {
namespace ui {

template <typename T>
class ResourceData;


namespace util {

template <typename F>
struct lambda_helper;

template <typename F, typename Result, typename T, typename... Args>
struct lambda_helper<Result (F::*)(ResourceData<T>&, Args...) const>
{
    using fn_type = std::function<Result(ResourceData<T>&, Args...)>;
};
} // namespace util


class ResourceFactory
{
private:
    struct RealizeFunctionBase
    {
        virtual ~RealizeFunctionBase() = default;
    };

    template <typename T, typename... Args>
    struct RealizeFunction : public RealizeFunctionBase
    {
        RealizeFunction(std::function<void(ResourceData<T>&, Args...)>&& fn_in)
            : fn(std::move(fn_in))
        {}

        std::function<void(ResourceData<T>&, Args...)> fn;
    };

    using KeyType = std::pair<std::type_index, std::type_index>;

public:
    /// Registers factory that simply forwards Args to T's constructor
    template <typename T, typename... Args>
    static void register_constructor_forwarding_factory()
    {
        register_resource_factory(
            [](ResourceData<T>& data, detail::convert_implicit_t<Args>... args) {
                data.set(detail::realize_forward<T, detail::convert_implicit_t<Args>...>(
                    std::forward<detail::convert_implicit_t<Args>>(args)...));
            });
    }

    /// Helper for std::function<> type deduction
    template <typename F>
    static void register_resource_factory(F&& realize_fn)
    {
        using h = util::lambda_helper<decltype(&F::operator())>;
        register_resource_factory_impl((typename h::fn_type)(realize_fn));
    }

    /// Registers a function to create given type
    /// function signature: void(ResourceData<T> &, Args ...)
    /// If any of the args is a rvalue (i.e., Arg && x), the value is moved from saved
    /// parameters and therefore the resource will not support reloading.
    template <typename T, typename... Args>
    static void register_resource_factory_impl(
        const std::function<void(ResourceData<T>&, Args... args)>& realize_fn)
    {
        /// Saves a factory function that uses saved copy of the arguments
        /// Factory function is stored with key made out of base non-qualified non-reference types
        using ParamPackDeferred = typename detail::args_to_tuple<Args...>::type;
        /// Original parameter pack for the factory function
        using ParamPackOriginal = std::tuple<detail::convert_implicit_t<Args>...>;

        const auto key =
            KeyType(std::type_index(typeid(T)), std::type_index(typeid(ParamPackDeferred)));

        // Check if it doesn't exist already
        auto it = m_deferred_realize_functions.find(key);
        if (it != m_deferred_realize_functions.end()) {
            return;
        }

        m_deferred_realize_functions[key] =
            std::make_unique<RealizeFunction<T>>([=](ResourceData<T>& data) -> void {
                // Wrap into a lambda so we can use std::apply later
                auto fn = [&](auto... params) {
                    realize_fn(data, std::forward<decltype(params)>(params)...);
                };

                detail::apply_parameters<ParamPackDeferred, ParamPackOriginal>(fn, data.params());
            });

        /// Saves a factory function that can use original arguments
        /// including their reference/qualifications
        m_direct_realize_functions[key] = std::make_unique<RealizeFunction<T, Args...>>(
            [=](ResourceData<T>& data, Args... args) -> void {
                realize_fn(data, std::forward<Args>(args)...);
            });
    }


    /// Deferred realization
    /// Uses realization function If BaseResourceData::params were defined and the realization function was registered
    /// Uses default constructor if no parameters were defined
    /// Throws if there were parameters defined but no realization function exists
    template <typename T>
    static void realize(ResourceData<T>& data)
    {
        // No params value, attempt default construction
        if (!data.params().has_value()) {
            // Throws if type is not default constructible
            data.set(detail::realize_default<T>());
            return;
        }

        const auto key = KeyType(std::type_index(typeid(T)), data.params().type());
        auto it = m_deferred_realize_functions.find(key);

        if (it == m_deferred_realize_functions.end()) {
            LA_ASSERT(
                false,
                lagrange::string_format(
                    "No realization function for resource (type = {}) parameter (type = "
                    "{}) combination",
                    typeid(T).name(),
                    data.params().type().name()));
        } else {
            auto fn_base_ptr = it->second.get();
            auto fn_ptr = reinterpret_cast<RealizeFunction<T>*>(fn_base_ptr);
            fn_ptr->fn(data);
        }
    }

    /// Direct realization
    /// Will use registered factory function if possible
    /// If not, it will simply forward the arguments to the T's constructor
    /// unique_ptr<T> and shared_ptr<T> can be used as an argument
    template <typename T, typename... Args>
    static void realize(ResourceData<T>& data, Args... args)
    {
        using ParamPackDeferred = typename detail::args_to_tuple<Args...>::type;

        const auto key =
            KeyType(std::type_index(typeid(T)), std::type_index(typeid(ParamPackDeferred)));

        auto it = m_direct_realize_functions.find(key);

        // No realization function, forward as is
        if (it == m_direct_realize_functions.end()) {
            data.set(detail::realize_forward<T, Args...>(std::forward<Args>(args)...));
        }
        // Custom realization function
        else {
            auto fn_base_ptr = it->second.get();
            auto fn_ptr =
                reinterpret_cast<RealizeFunction<T, detail::convert_implicit_t<Args>...>*>(
                    fn_base_ptr);
            fn_ptr->fn(data, std::forward<Args>(args)...);
        }
    }


    static void clear()
    {
        m_direct_realize_functions.clear();
        m_deferred_realize_functions.clear();
    }

private:
    static std::unordered_map<KeyType, std::unique_ptr<RealizeFunctionBase>, utils::pair_hash>
        m_deferred_realize_functions;

    static std::unordered_map<KeyType, std::unique_ptr<RealizeFunctionBase>, utils::pair_hash>
        m_direct_realize_functions;
};


} // namespace ui
} // namespace lagrange

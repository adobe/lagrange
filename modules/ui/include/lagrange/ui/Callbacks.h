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

#include <functional>
#include <vector>

namespace lagrange {
namespace ui {

/*
    Callback function wrapper with counter.
    Counter is used so that callbacks of same
    signature are treated as different types.
*/
template <typename T, int c = 0>
struct Callback {
    using ArgType = T;
    using FunType = std::function<T>;

    std::function<T> m_fun;
};


#define UI_CALLBACK(T) Callback<T, __LINE__>

/*
    Container of callbacks specified by Args
*/
template <typename... Args>
class Callbacks {
    template <typename... TupleArgs>
    struct CallbacksTuple {
        std::tuple<TupleArgs...> callbacks;
    };

public:
    template <typename CallbackType>
    const CallbackType& add(const CallbackType& callback)
    {
        using CallbackTypeNoRef = std::decay_t<CallbackType>;
        auto& arr = std::get<std::vector<CallbackTypeNoRef>>(m_base.callbacks);
        arr.emplace_back(callback);
        return arr.back();
    }

    template <typename CallbackType>
    void add(CallbackType&& callback)
    {
        using CallbackTypeNoRef = std::decay_t<CallbackType>;
        std::get<std::vector<CallbackTypeNoRef>>(m_base.callbacks)
            .emplace_back(std::move(callback));
    }

    template <typename CallbackType>
    void clear()
    {
        using CallbackTypeNoRef = std::decay_t<CallbackType>;
        std::get<std::vector<CallbackTypeNoRef>>(m_base.callbacks).clear();
    }

    template <typename CallbackType>
    bool erase(size_t index)
    {
        using CallbackTypeNoRef = std::decay_t<CallbackType>;
        auto& vec = std::get<std::vector<CallbackTypeNoRef>>(m_base.callbacks);
        if (index >= vec.size()) return false;
        vec.erase(vec.begin() + index);
        return true;
    }
    template <typename CallbackType>
    size_t size()
    {
        using CallbackTypeNoRef = std::decay_t<CallbackType>;
        return std::get<std::vector<CallbackTypeNoRef>>(m_base.callbacks).size();
    }


    template <typename CallbackType, typename... CallbackArgs>
    void call(CallbackArgs&&... args) const
    {
        if (!m_enabled) return;
        using CallbackTypeNoRef = std::decay_t<CallbackType>;
        for (auto& callback : std::get<std::vector<CallbackTypeNoRef>>(m_base.callbacks)) {
            callback.m_fun(args...);
        }
    }

    template <typename CallbackType>
    bool has_callback()
    {
        using CallbackTypeNoRef = std::decay_t<CallbackType>;
        return std::get<std::vector<CallbackTypeNoRef>>(m_base.callbacks).size() > 0;
    }


    void set_enabled(bool enabled) { m_enabled = enabled; }

    bool is_enabled() const { return m_enabled; }

private:
    CallbacksTuple<std::vector<std::decay_t<Args>>...> m_base;
    bool m_enabled = true;
};

/*
 * Defines the boilerplate implementations for the `derived()` methods normally
 * included in a CRTP base class.
 *
 * NOTE: This class is a direct copy from optimtools. Maybe in the future we
 * will want to include it as dependency
 * see e.g.,
 * https://git.corp.adobe.com/kwampler/optimtools/blob/4a1390515d9880a4de14b8b8feca9d2c31f4fe3f/source/optimtools/general_utils.h#L17
 */
template <class Derived>
class CRTPBase {
public:
    inline const Derived& derived() const& { return static_cast<const Derived&>(*this); }
    inline Derived& derived() & { return static_cast<Derived&>(*this); }
    inline Derived&& derived() && { return std::move(static_cast<Derived&>(*this)); }
};

/*
 * Base class that uses CRTP to reduce code duplication in the derived classes
 *
 * CRTP ref: https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
 */
template <typename T>
class CallbacksBase : public CRTPBase<T> {
public:
    template <typename CallbackType>
    void add_callback(typename CallbackType::FunType&& fun)
    {
        this->derived().m_callbacks.template add<CallbackType>(CallbackType{fun});
    }
    template <typename CallbackType>
    void clear_callback()
    {
        this->derived().m_callbacks.template clear<CallbackType>();
    }
    template <typename CallbackType>
    bool has_callback()
    {
        return this->derived().m_callbacks.template has_callback<CallbackType>();
    }
    void set_enabled(const bool enabled) { this->derived().m_callbacks.set_enabled(enabled); }
};
} // namespace ui
} // namespace lagrange

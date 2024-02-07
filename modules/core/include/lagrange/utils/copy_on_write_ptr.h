/*
 * Copyright 2022 Adobe. All rights reserved.
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

#include <lagrange/Logger.h>

#include <lagrange/internal/shared_ptr.h>
#include <lagrange/utils/build.h>

#include <cassert>
#include <memory>
#include <mutex>

namespace lagrange {

/// @addtogroup group-utils-misc
/// @{

///
/// A handle type with copy-on-write semantics. Any copy of the handle will share data ownership,
/// causing potential copies of the data on write access.
///
/// @tparam     T     Pointee type.
///
template <typename T>
class copy_on_write_ptr
{
public:
    using pointer = T*;
    using const_pointer = typename std::add_const<T>::type*;
    using reference = T&;
    using const_reference = typename std::add_const<T>::type&;
    using element_type = T;

public:
    /// Construct a copy-on-write ptr from a shared-pointer
    copy_on_write_ptr(::lagrange::internal::shared_ptr<T>&& ptr = nullptr)
        : m_data(std::move(ptr))
    {
        if (m_data) {
            la_runtime_assert(m_data.use_count() == 1);
        }
    }

    /// Default move constructor.
    copy_on_write_ptr(copy_on_write_ptr&&) = default;

    /// Default move assignment operator.
    copy_on_write_ptr& operator=(copy_on_write_ptr&&) = default;

    /// Move-construct from a derived type.
    template <typename Derived>
    copy_on_write_ptr(copy_on_write_ptr<Derived>&& ptr)
        : m_data{std::move(ptr.m_data)}
    {}

    /// Default copy constructor.
    copy_on_write_ptr(const copy_on_write_ptr&) = default;

    /// Default copy assignment operator.
    copy_on_write_ptr& operator=(const copy_on_write_ptr&) = default;

    /// Check if this copy-on-write ptr is nullptr.
    explicit operator bool() const { return bool(m_data); }

    /// Returns a const pointer to the data. Does not require ownership and will not lead to any copy.
    const T* read() const { return m_data.get(); }

    /// Returns a const pointer to the data. Does not require ownership and will not lead to any copy.
    template <typename Derived>
    const Derived* static_read() const
    {
        return static_cast<const Derived*>(m_data.get());
    }

    /// Returns a writable pointer to the data. Will cause a copy if ownership is shared.
    template <typename Derived>
    Derived* static_write()
    {
        ensure_unique_owner<Derived>();
        return static_cast<Derived*>(m_data.get());
    }

    /// Returns a writable smart pointer to the data. Will cause a copy if ownership is shared.
    template <typename Derived>
    std::shared_ptr<Derived> release_ptr()
    {
        ensure_unique_owner<Derived>();
        auto ptr = static_cast<Derived*>(m_data.get());
#if LAGRANGE_TARGET_FEATURE(RTTI)
        la_debug_assert(dynamic_cast<Derived*>(m_data.get()));
#endif
        auto ret = std::make_shared<Derived>(std::move(*ptr));
        m_data.reset();
        return ret;
    }

public:
    /// Return a weak pointer to the data.
    ///
    /// @warning This method is for internal usage purposes.
    ///
    /// @see read() for preferred way of accessing data.
    ::lagrange::internal::weak_ptr<const T> _get_weak_ptr() const { return m_data; }

    /// Return a weak pointer to the data.
    ///
    /// @warning This method is for internal usage purposes.
    ///
    /// @see read() for preferred way of accessing data.
    ::lagrange::internal::weak_ptr<T> _get_weak_ptr() { return m_data; }

protected:
    ::lagrange::internal::shared_ptr<T> m_data;

    /// Protect from data races when acquiring ownership to the same object from multiple threads
    // std::mutex m_ownership_mutex;

    /// If we are not the owner of the shared object, make a private copy of it
    template <typename Derived>
    void ensure_unique_owner()
    {
        // Only mutex lock if we are not the owner. This prevents having to lock mutexes when we are
        // already the unique owner of the data.
        if (m_data.use_count() != 1) {
            // TODO: We should be using a mutex here, but this causes the copy-on-write pointer to
            // not be movable/copyable. We need to fix this before we can use a mutex.
            // std::lock_guard<std::mutex> lock(m_ownership_mutex);
            // Test again in protected section to ensure we don't copy the data twice for the same
            // object.
            // if (m_data.use_count() != 1) {
                auto ptr = static_cast<const Derived*>(m_data.get());
#if LAGRANGE_TARGET_FEATURE(RTTI)
                la_debug_assert(dynamic_cast<const Derived*>(m_data.get()));
#endif
                m_data = ::lagrange::internal::make_shared<Derived>(*ptr);
            // }
        }
    }
};

/// @}

} // namespace lagrange

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

#include <memory>

#include <lagrange/utils/span.h>

namespace lagrange {

/// @addtogroup group-utils-misc
/// @{

/**
 * Shared span with ownership tracking.
 *
 * Sometimes the buffer referred by a span is already using certain kind of memory ownership sharing
 * schemes (e.g. `shared_ptr` or PyObject).  The SharedSpan class can be used to keep the buffer
 * alive by taking a shared of the ownership of the buffer.
 *
 * @tparam T  Value type.
 *
 * @see @ref make_shared_span
 */
template <typename T>
class SharedSpan
{
public:
    using element_type = T;
    using value_type = std::remove_cv_t<T>;

public:
    /**
     * Default constructor for empty buffer.
     */
    SharedSpan()
        : m_data(nullptr)
        , m_size(0)
    {}

    /**
     * Constructing a SharedSpan object from a shared buffer.
     *
     * @param data  The owner of the shared buffer.
     * @param size  The size of the shared buffer.
     */
    SharedSpan(std::shared_ptr<T> data, size_t size)
        : m_data(data)
        , m_size(size)
    {}

    /**
     * @return A writable span of the shared buffer.
     */
    span<T> ref() { return {m_data.get(), m_size}; }

    /**
     * @return A readonly span of the shared buffer.
     */
    span<const T> get() const { return {m_data.get(), m_size}; }

    /**
     * @return The memory owner of the shared buffer.
     */
    std::shared_ptr<const T> owner() const { return m_data; }

    /**
     * @return The buffer size.
     */
    size_t size() const { return m_size; }

protected:
    // Technically, we should be using `std::shared_ptr<T[]>`.  However, some versions of Apple
    // Clang (e.g. clang-1200.0.32.28) has a bug in inferring the correct `element_type`.
    std::shared_ptr<T> m_data;
    size_t m_size = 0;
};

/**
 * Created a SharedSpan object around an internal buffer of a parent object.
 *
 * @tparam T  The `element_type` of internal buffer.
 * @tparam Y  The type of the parent object containing the buffer.
 *
 * @param[in] r            A shared pointer of the parent object owning the buffer.
 * @param[in] element_ptr  A raw pointer to the internal buffer contained in the parent object.
 * @param[in] size         The size of the internal buffer.
 *
 * @return A SharedSpan that shares the memory ownership of the internal buffer.
 */
template <typename T, typename Y>
SharedSpan<T> make_shared_span(const std::shared_ptr<Y>& r, T* element_ptr, size_t size)
{
    return {std::shared_ptr<T>(r, element_ptr), size};
}

/// @}

} // namespace lagrange

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
#include <lagrange/Attribute.h>

#include <lagrange/AttributeTypes.h>
#include <lagrange/Logger.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/warning.h>

#include <cstddef>

namespace lagrange {

////////////////////////////////////////////////////////////////////////////////
// Base class
////////////////////////////////////////////////////////////////////////////////

AttributeBase::AttributeBase(AttributeElement element, AttributeUsage usage, size_t num_channels)
    : m_element(element)
    , m_usage(usage)
    , m_num_channels(num_channels)
{
    switch (usage) {
    case AttributeUsage::Vector: la_runtime_assert(num_channels >= 1); break;
    case AttributeUsage::Scalar: la_runtime_assert(num_channels == 1); break;
    case AttributeUsage::Normal:
    case AttributeUsage::Tangent:
    case AttributeUsage::Bitangent:
        la_runtime_assert(num_channels >= 1);
        break; // depends on mesh dimension
    case AttributeUsage::Color: la_runtime_assert(num_channels >= 1 && num_channels <= 4); break;
    case AttributeUsage::UV: la_runtime_assert(num_channels == 2); break;
    case AttributeUsage::VertexIndex: la_runtime_assert(num_channels == 1); break;
    case AttributeUsage::FacetIndex: la_runtime_assert(num_channels == 1); break;
    case AttributeUsage::CornerIndex: la_runtime_assert(num_channels == 1); break;
    case AttributeUsage::EdgeIndex: la_runtime_assert(num_channels == 1); break;
    default: throw Error("Unsupported usage");
    }
}

AttributeBase::~AttributeBase() = default;

////////////////////////////////////////////////////////////////////////////////
// Constructors
////////////////////////////////////////////////////////////////////////////////

template <typename ValueType>
Attribute<ValueType>::Attribute(AttributeElement element, AttributeUsage usage, size_t num_channels)
    : AttributeBase(element, usage, num_channels)
{
    // Index attributes must have integral types
    LA_IGNORE_SWITCH_ENUM_WARNING_BEGIN
    switch (usage) {
    case AttributeUsage::VertexIndex: la_runtime_assert(std::is_integral_v<ValueType>); break;
    case AttributeUsage::FacetIndex: la_runtime_assert(std::is_integral_v<ValueType>); break;
    case AttributeUsage::CornerIndex: la_runtime_assert(std::is_integral_v<ValueType>); break;
    case AttributeUsage::EdgeIndex: la_runtime_assert(std::is_integral_v<ValueType>); break;
    case AttributeUsage::Vector: break;
    case AttributeUsage::Scalar: break;
    case AttributeUsage::Normal: break;
    case AttributeUsage::Tangent: break;
    case AttributeUsage::Bitangent: break;
    case AttributeUsage::Color: break;
    case AttributeUsage::UV: break;
    default: throw Error("Unsupported usage");
    }
    LA_IGNORE_SWITCH_ENUM_WARNING_END
}

template <typename ValueType>
Attribute<ValueType>::~Attribute() = default;

template <typename ValueType>
Attribute<ValueType>::Attribute(Attribute<ValueType>&& other) noexcept
    : AttributeBase(std::move(other))
    , m_data(std::move(other.m_data))
    , m_owner(std::move(other.m_owner))
    , m_default_value(other.m_default_value)
    , m_view(other.m_view)
    , m_const_view(other.m_const_view)
    , m_growth_policy(other.m_growth_policy)
    , m_write_policy(other.m_write_policy)
    , m_copy_policy(other.m_copy_policy)
    , m_is_external(other.m_is_external)
    , m_is_read_only(other.m_is_read_only)
    , m_num_elements(other.m_num_elements)
{
    other.clear_views();
    if (!is_external()) {
        // It's a move, so internal buffer address should be the same
        la_debug_assert(m_data.data() == m_view.data());
        la_debug_assert(m_data.data() == m_const_view.data());
    }
}

template <typename ValueType>
Attribute<ValueType>& Attribute<ValueType>::operator=(Attribute<ValueType>&& other) noexcept
{
    if (this != &other) {
        AttributeBase::operator=(std::move(other));
        m_data = std::move(other.m_data);
        m_default_value = other.m_default_value;
        m_view = other.m_view;
        m_const_view = other.m_const_view;
        m_growth_policy = other.m_growth_policy;
        m_write_policy = other.m_write_policy;
        m_copy_policy = other.m_copy_policy;
        m_is_external = other.m_is_external;
        m_is_read_only = other.m_is_read_only;
        m_num_elements = other.m_num_elements;
        m_owner = std::move(other.m_owner);
        other.clear_views();
        if (!is_external()) {
            // It's a move, so internal buffer address should be the same
            la_debug_assert(m_data.data() == m_view.data());
            la_debug_assert(m_data.data() == m_const_view.data());
        }
    }
    return *this;
}

template <typename ValueType>
Attribute<ValueType>::Attribute(const Attribute<ValueType>& other)
    : AttributeBase(other)
    , m_data(other.m_data)
    , m_owner(other.m_owner)
    , m_default_value(other.m_default_value)
    , m_view(other.m_view)
    , m_const_view(other.m_const_view)
    , m_growth_policy(other.m_growth_policy)
    , m_write_policy(other.m_write_policy)
    , m_copy_policy(other.m_copy_policy)
    , m_is_external(other.m_is_external)
    , m_is_read_only(other.m_is_read_only)
    , m_num_elements(other.m_num_elements)
{
    if (!is_external()) {
        // It's a copy, so internal buffer address should be updated
        update_views();
    } else {
        switch (m_copy_policy) {
        case AttributeCopyPolicy::CopyIfExternal: create_internal_copy(); break;
        case AttributeCopyPolicy::KeepExternalPtr: break;
        case AttributeCopyPolicy::ErrorIfExternal:
            throw Error("Attribute copy policy prevents copying external buffer");
        }
    }
}

template <typename ValueType>
Attribute<ValueType>& Attribute<ValueType>::operator=(const Attribute<ValueType>& other)
{
    if (this != &other) {
        AttributeBase::operator=(other);
        m_data = other.m_data;
        m_default_value = other.m_default_value;
        m_view = other.m_view;
        m_const_view = other.m_const_view;
        m_growth_policy = other.m_growth_policy;
        m_write_policy = other.m_write_policy;
        m_copy_policy = other.m_copy_policy;
        m_is_external = other.m_is_external;
        m_is_read_only = other.m_is_read_only;
        m_num_elements = other.m_num_elements;
        m_owner = other.m_owner;
        if (!is_external()) {
            // It's a copy, so internal buffer address should be updated
            update_views();
        } else {
            switch (m_copy_policy) {
            case AttributeCopyPolicy::CopyIfExternal: create_internal_copy(); break;
            case AttributeCopyPolicy::KeepExternalPtr: break;
            case AttributeCopyPolicy::ErrorIfExternal:
                throw Error("Attribute copy policy prevents copying external buffer");
            }
        }
    }
    return *this;
}

template <typename ValueType>
void Attribute<ValueType>::wrap(lagrange::span<ValueType> buffer, size_t num_elements)
{
    la_runtime_assert(num_elements * get_num_channels() <= buffer.size());
    m_view = buffer;
    m_const_view = buffer;
    m_num_elements = num_elements;
    m_data.clear();
    m_is_external = true;
    m_is_read_only = false;
    m_owner.reset();
}

template <typename ValueType>
void Attribute<ValueType>::wrap(SharedSpan<ValueType> shared_buffer, size_t num_elements)
{
    m_view = shared_buffer.ref();
    m_const_view = m_view;
    m_num_elements = num_elements;
    m_data.clear();
    m_owner = shared_buffer.owner();
    m_is_external = true;
    m_is_read_only = false;
}

template <typename ValueType>
void Attribute<ValueType>::wrap_const(lagrange::span<const ValueType> buffer, size_t num_elements)
{
    la_runtime_assert(num_elements * get_num_channels() <= buffer.size());
    m_view = {};
    m_const_view = buffer;
    m_num_elements = num_elements;
    m_data.clear();
    m_is_external = true;
    m_is_read_only = true;
    m_owner.reset();
}

template <typename ValueType>
void Attribute<ValueType>::wrap_const(
    SharedSpan<const ValueType> shared_buffer,
    size_t num_elements)
{
    m_view = {};
    m_const_view = shared_buffer.get();
    m_num_elements = num_elements;
    m_data.clear();
    m_owner = shared_buffer.owner();
    m_is_external = true;
    m_is_read_only = true;
}

////////////////////////////////////////////////////////////////////////////////
// Data modification
////////////////////////////////////////////////////////////////////////////////

template <typename ValueType>
void Attribute<ValueType>::create_internal_copy()
{
    // Reserve a buffer with the same capacity as the external buffer, but only copy/assign the
    // actual values currently in use. This allows padding with extra numbers (e.g. adding a 4th
    // float coordinate past the last vertex for Embree).
    la_runtime_assert(is_external());
    m_data.reserve(m_const_view.size());
    m_data.assign(get_all().begin(), get_all().end());
    la_debug_assert(m_data.capacity() == m_const_view.size());
    m_is_external = false;
    m_is_read_only = false;
    m_owner.reset();
    update_views();
}

template <typename ValueType>
void Attribute<ValueType>::clear()
{
    growth_check(0);
    if (!is_external()) {
        m_data.clear();
        update_views();
    } else {
        m_num_elements = 0;
    }
}

template <typename ValueType>
void Attribute<ValueType>::shrink_to_fit()
{
    if (is_external()) {
        auto new_cap = get_num_elements() * get_num_channels();
        if (new_cap == m_const_view.size()) {
            // Capacity is exactly the number of entries: nothing to do
            return;
        }
        la_debug_assert(new_cap <= m_const_view.size());
        // Capacity is too big for the number of entries: check shrink policy
        switch (m_shrink_policy) {
        case AttributeShrinkPolicy::ErrorIfExternal:
            throw Error("Attribute policy prevents shrinking external buffer");
        case AttributeShrinkPolicy::IgnoreIfExternal: break;
        case AttributeShrinkPolicy::WarnAndCopy:
            logger().warn("Requested growth of an attribute pointing to external data. An internal "
                          "copy will be created.");
            [[fallthrough]];
        case AttributeShrinkPolicy::SilentCopy:
            // Avoid reserving extra elements in the newly allocated internal buffer
            m_const_view = {m_const_view.data(), new_cap};
            // Now allocate the actual internal copy
            create_internal_copy();
            break;
        default: throw Error("Unsupported case");
        }
    } else {
        m_data.shrink_to_fit();
        update_views();
    }
}

template <typename ValueType>
void Attribute<ValueType>::reserve_entries(size_t new_cap)
{
    growth_check(new_cap);
    if (!is_external()) {
        m_data.reserve(new_cap);
        update_views();
    }
}

template <typename ValueType>
void Attribute<ValueType>::resize_elements(size_t num_elements)
{
    growth_check(num_elements * get_num_channels());
    if (!is_external()) {
        m_data.resize(num_elements * get_num_channels(), m_default_value);
        update_views();
    } else {
        if (num_elements > m_num_elements) {
            write_check();
            auto num_added = (num_elements - m_num_elements) * get_num_channels();
            auto span = m_view.subspan(m_num_elements * get_num_channels(), num_added);
            std::fill(span.begin(), span.end(), m_default_value);
        }
        m_num_elements = num_elements;
    }
}

template <typename ValueType>
void Attribute<ValueType>::insert_elements(lagrange::span<const ValueType> values)
{
    la_runtime_assert(values.size() % get_num_channels() == 0);
    growth_check(get_num_elements() * get_num_channels() + values.size());
    if (!is_external()) {
        m_data.insert(m_data.end(), values.begin(), values.end());
        update_views();
    } else {
        write_check();
        auto span = m_view.subspan(m_num_elements * get_num_channels(), values.size());
        std::copy(values.begin(), values.end(), span.begin());
        m_num_elements += values.size() / get_num_channels();
    }
}

template <typename ValueType>
void Attribute<ValueType>::insert_elements(std::initializer_list<const ValueType> values)
{
    insert_elements(span<const ValueType>(values.begin(), values.end()));
}

template <typename ValueType>
void Attribute<ValueType>::insert_elements(size_t count)
{
    growth_check((get_num_elements() + count) * get_num_channels());
    if (!is_external()) {
        m_data.insert(m_data.end(), count * get_num_channels(), m_default_value);
        update_views();
    } else {
        write_check();
        auto num_added = count * get_num_channels();
        auto span = m_view.subspan(m_num_elements * get_num_channels(), num_added);
        std::fill(span.begin(), span.end(), m_default_value);
        m_num_elements += count;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Data access
////////////////////////////////////////////////////////////////////////////////

template <typename ValueType>
ValueType Attribute<ValueType>::get(size_t i, size_t c) const
{
    return m_const_view[i * get_num_channels() + c];
}

template <typename ValueType>
ValueType& Attribute<ValueType>::ref(size_t i, size_t c)
{
    write_check();
    return m_view[i * get_num_channels() + c];
}

template <typename ValueType>
ValueType Attribute<ValueType>::get(size_t i) const
{
    la_runtime_assert(get_num_channels() == 1);
    return m_const_view[i];
}

template <typename ValueType>
ValueType& Attribute<ValueType>::ref(size_t i)
{
    la_runtime_assert(get_num_channels() == 1);
    write_check();
    return m_view[i];
}

template <typename ValueType>
lagrange::span<const ValueType> Attribute<ValueType>::get_all() const
{
    return m_const_view.first(m_num_elements * get_num_channels());
}

template <typename ValueType>
lagrange::span<ValueType> Attribute<ValueType>::ref_all()
{
    write_check();
    return m_view.first(m_num_elements * get_num_channels());
}

template <typename ValueType>
lagrange::span<const ValueType> Attribute<ValueType>::get_first(size_t num_elements) const
{
    return get_all().first(num_elements * get_num_channels());
}

template <typename ValueType>
lagrange::span<ValueType> Attribute<ValueType>::ref_first(size_t num_elements)
{
    return ref_all().first(num_elements * get_num_channels());
}

template <typename ValueType>
lagrange::span<const ValueType> Attribute<ValueType>::get_last(size_t num_elements) const
{
    return get_all().last(num_elements * get_num_channels());
}

template <typename ValueType>
lagrange::span<ValueType> Attribute<ValueType>::ref_last(size_t num_elements)
{
    return ref_all().last(num_elements * get_num_channels());
}

template <typename ValueType>
lagrange::span<const ValueType> Attribute<ValueType>::get_middle(
    size_t first_element,
    size_t num_elements) const
{
    return get_all().subspan(first_element * get_num_channels(), num_elements * get_num_channels());
}

template <typename ValueType>
lagrange::span<ValueType> Attribute<ValueType>::ref_middle(
    size_t first_element,
    size_t num_elements)
{
    return ref_all().subspan(first_element * get_num_channels(), num_elements * get_num_channels());
}

template <typename ValueType>
lagrange::span<const ValueType> Attribute<ValueType>::get_row(size_t element) const
{
    return get_all().subspan(element * get_num_channels(), get_num_channels());
}

template <typename ValueType>
lagrange::span<ValueType> Attribute<ValueType>::ref_row(size_t element)
{
    return ref_all().subspan(element * get_num_channels(), get_num_channels());
}

////////////////////////////////////////////////////////////////////////////////
// Protected methods
////////////////////////////////////////////////////////////////////////////////

template <typename ValueType>
void Attribute<ValueType>::growth_check(size_t new_cap)
{
    if (is_external()) {
        if (new_cap == get_num_elements() * get_num_channels()) {
            // Size didn't change: do not complain.
            return;
        }
        // Size changed (either increase or decrease): error or warning + internal copy.
        switch (m_growth_policy) {
        case AttributeGrowthPolicy::ErrorIfExternal:
            throw Error("Attribute policy prevents growing external buffer");
        case AttributeGrowthPolicy::AllowWithinCapacity:
            if (new_cap > m_const_view.size()) {
                throw Error(fmt::format(
                    "Attribute policy prevents growing external buffer beyond capacity ({} / {})",
                    new_cap,
                    m_const_view.size()));
            }
            break;
        case AttributeGrowthPolicy::WarnAndCopy:
            logger().warn("Requested growth of an attribute pointing to external data. An internal "
                          "copy will be created.");
            [[fallthrough]];
        case AttributeGrowthPolicy::SilentCopy: create_internal_copy(); break;
        default: throw Error("Unsupported case");
        }
    }
}

template <typename ValueType>
void Attribute<ValueType>::write_check()
{
    if (is_read_only()) {
        switch (m_write_policy) {
        case AttributeWritePolicy::ErrorIfReadOnly:
            throw Error("Attribute policy prevents writing to a read-only buffer");
        case AttributeWritePolicy::WarnAndCopy:
            logger().warn("Requested write access to an attribute pointing to read-only data. An "
                          "internal copy will be created.");
            [[fallthrough]];
        case AttributeWritePolicy::SilentCopy: create_internal_copy(); break;
        default: throw Error("Unsupported case");
        }
    }
}

template <typename ValueType>
void Attribute<ValueType>::update_views()
{
    la_debug_assert(!is_external());
    la_debug_assert(m_owner == nullptr);
    la_debug_assert(get_num_channels() != 0);
    la_debug_assert(m_data.size() % get_num_channels() == 0);
    m_view = {m_data.data(), m_data.size()};
    m_const_view = {m_data.data(), m_data.size()};
    m_num_elements = m_const_view.size() / get_num_channels();
}

template <typename ValueType>
void Attribute<ValueType>::clear_views()
{
    m_view = {};
    m_const_view = {};
    m_num_elements = m_data.size() / get_num_channels();
}

////////////////////////////////////////////////////////////////////////////////
// Explicit template instantiation
////////////////////////////////////////////////////////////////////////////////

#define LA_X_attr(_, ValueType) template class Attribute<ValueType>;
LA_ATTRIBUTE_X(attr, 0)

} // namespace lagrange

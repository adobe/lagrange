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
#include <lagrange/IndexedAttribute.h>

#include <lagrange/AttributeTypes.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>

#include <cstddef>

namespace lagrange {

////////////////////////////////////////////////////////////////////////////////
// Constructors
////////////////////////////////////////////////////////////////////////////////

template <typename ValueType, typename Index>
IndexedAttribute<ValueType, Index>::IndexedAttribute(AttributeUsage usage, size_t num_channels)
    : AttributeBase(AttributeElement::Indexed, usage, num_channels)
    , m_values(AttributeElement::Value, usage, num_channels)
    , m_indices(AttributeElement::Corner, AttributeUsage::Scalar, 1)
{}

template <typename ValueType, typename Index>
IndexedAttribute<ValueType, Index>::~IndexedAttribute() = default;

template <typename ValueType, typename Index>
IndexedAttribute<ValueType, Index>::IndexedAttribute(
    IndexedAttribute<ValueType, Index>&& other) noexcept
    : AttributeBase(std::move(other))
    , m_values(std::move(other.m_values))
    , m_indices(std::move(other.m_indices))
{}

template <typename ValueType, typename Index>
IndexedAttribute<ValueType, Index>& IndexedAttribute<ValueType, Index>::operator=(
    IndexedAttribute<ValueType, Index>&& other) noexcept
{
    if (this != &other) {
        AttributeBase::operator=(std::move(other));
        m_values = std::move(other.m_values);
        m_indices = std::move(other.m_indices);
    }
    return *this;
}

template <typename ValueType, typename Index>
IndexedAttribute<ValueType, Index>::IndexedAttribute(
    const IndexedAttribute<ValueType, Index>& other)
    : AttributeBase(other)
    , m_values(other.m_values)
    , m_indices(other.m_indices)
{}

template <typename ValueType, typename Index>
IndexedAttribute<ValueType, Index>& IndexedAttribute<ValueType, Index>::operator=(
    const IndexedAttribute<ValueType, Index>& other)
{
    if (this != &other) {
        AttributeBase::operator=(other);
        m_values = other.m_values;
        m_indices = other.m_indices;
    }
    return *this;
}

////////////////////////////////////////////////////////////////////////////////
// Explicit template instantiation
////////////////////////////////////////////////////////////////////////////////

#define LA_X_attr(Index, ValueType) template class IndexedAttribute<ValueType, Index>;
#define LA_X_mesh(_, Index) LA_ATTRIBUTE_X(attr, Index)
LA_SURFACE_MESH_INDEX_X(mesh, 0)

} // namespace lagrange

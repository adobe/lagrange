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

#include <lagrange/Attribute.h>

namespace lagrange {

/// @addtogroup group-surfacemesh-attr
/// @{

///
/// Derived attribute class that stores the actual information.
///
/// @tparam     ValueType_  Attribute value type.
/// @tparam     Index_      Attribute index type.
///
template <typename ValueType_, typename Index_>
class IndexedAttribute : public AttributeBase
{
public:
    /// Attribute value type.
    using ValueType = ValueType_;

    /// Attribute index type.
    using Index = Index_;

    /// Whether this attribute type is indexed.
    static constexpr bool IsIndexed = true;

public:
    ///
    /// @name Attribute construction
    /// @{

    ///
    /// Constructs a new instance.
    ///
    /// @param[in]  usage         Usage tag.
    /// @param[in]  num_channels  Number of channels.
    ///
    IndexedAttribute(AttributeUsage usage, size_t num_channels);

    ///
    /// Destroys the object.
    ///
    ~IndexedAttribute() override;

    ///
    /// Move constructor.
    ///
    /// @param      other  Instance to move from.
    ///
    IndexedAttribute(IndexedAttribute&& other) noexcept;

    ///
    /// Assignment move operator.
    ///
    /// @param      other  Instance to move from.
    ///
    /// @return     The result of the assignment.
    ///
    IndexedAttribute& operator=(IndexedAttribute&& other) noexcept;

    ///
    /// Copy constructor.
    ///
    /// @param[in]  other  Instance to copy from.
    ///
    explicit IndexedAttribute(const IndexedAttribute& other);

    ///
    /// Assignment copy operator.
    ///
    /// @param[in]  other  Instance to copy from.
    ///
    /// @return     The result of the assignment.
    ///
    IndexedAttribute& operator=(const IndexedAttribute& other);

    ///
    /// Gets the attribute value type.
    ///
    /// @return     An enum describing the value type.
    ///
    [[nodiscard]] AttributeValueType get_value_type() const override
    {
        return m_values.get_value_type();
    }

    /// @}
    /// @name Attribute access
    /// @{

    ///
    /// Access attribute values.
    ///
    /// @return     A reference to the attribute values.
    ///
    Attribute<ValueType>& values() { return m_values; }

    ///
    /// Access attribute values.
    ///
    /// @return     A reference to the attribute values.
    ///
    const Attribute<ValueType>& values() const { return m_values; }

    ///
    /// Access attribute indices.
    ///
    /// @return     A reference to the attribute indices.
    ///
    Attribute<Index>& indices() { return m_indices; }

    ///
    /// Access attribute indices.
    ///
    /// @return     A reference to the attribute indices.
    ///
    const Attribute<Index>& indices() const { return m_indices; }

    /// @}

protected:
    Attribute<ValueType> m_values;
    Attribute<Index> m_indices;

    // TODO:
    // copy_on_write_ptr<Attribute<ValueType>> m_values;
    // copy_on_write_ptr<Attribute<Index>> m_indices;
};

/// @}

} // namespace lagrange

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

#include <limits>
#include <type_traits>

namespace lagrange {

///
/// @ingroup    group-utils
///
/// Bit field utility class.
///
/// @{

///
/// This class describes a bit field over an enum type.
///
/// @tparam     EnumType_  An enum type whose values are to be used in the bit field.
///
template <typename EnumType_>
class BitField
{
public:
    /// Enum type on top of which the bit field is built.
    using EnumType = EnumType_;

    /// Underlying integral type representing the enum type.
    using UnderlyingType = std::underlying_type_t<EnumType>;

    /// Default Constructor
    constexpr BitField()
        : m_bits(0)
    {}

    ///
    /// Constructor.
    ///
    /// @param[in]  value  Enum to initialize the bit field with.
    ///
    constexpr BitField(EnumType value)
        : m_bits(static_cast<UnderlyingType>(value))
    {}

    ///
    /// Constructor.
    ///
    /// @param[in]  bits  Value to initialize the bit field with.
    ///
    constexpr BitField(UnderlyingType bits)
        : m_bits(bits)
    {}

    ///
    /// Named constructor returning a bitfield set to 0.
    ///
    /// @return     The bit field.
    ///
    constexpr static BitField none() { return BitField(); }

    ///
    /// Named constructor returning a bitfield set to 1.
    ///
    /// @return     The bit field.
    ///
    constexpr static BitField all() { return ~BitField(); }

    ///
    /// Set to 1 the specified bit.
    ///
    /// @param[in]  other  The bits to set.
    ///
    constexpr void set(const BitField& other) { m_bits |= other.m_bits; }

    ///
    /// Set to 0 the specified Bit
    ///
    /// @param[in]  other  The bits to clear.
    ///
    constexpr void clear(const BitField& other) { m_bits &= ~other.m_bits; }

    ///
    /// Set all bits to 0.
    ///
    constexpr void clear_all() { m_bits = UnderlyingType(0); }

    ///
    /// Test the specified bit.
    ///
    /// @param[in]  other  The bits to test.
    ///
    /// @return     True if all the specified bits are set to 1.
    ///
    constexpr bool test(const BitField& other) const
    {
        return (m_bits & other.m_bits) == other.m_bits;
    }

    ///
    /// Test if any bits are set.
    ///
    /// @param[in]  other  The bits to test.
    ///
    /// @return     true if any of the specified bits is set to 1
    ///
    constexpr bool test_any(const BitField& other) const { return (m_bits & other.m_bits) != 0; }

    ///
    /// Allow to set the value of a bit.
    ///
    /// @param[in]  other   The bits to modify.
    /// @param[in]  is_set  The value that indicates if the bit is set or not.
    ///
    constexpr void set_bit(const BitField& other, bool is_set)
    {
        if (is_set) {
            set(other);
        } else {
            clear(other);
        }
    }

    ///
    /// Implicit conversion to the underlying integer type.
    ///
    constexpr operator UnderlyingType() const { return m_bits; }

    ///
    /// Gets the underlying integral value.
    ///
    /// @return     Internal value.
    ///
    constexpr const UnderlyingType& get_value() const { return m_bits; }

    ///
    /// Equal operator.
    ///
    /// @param[in]  other  The other bitfield.
    ///
    /// @return     true if other is equal to current.
    ///
    constexpr bool operator==(const BitField& other) const { return m_bits == other.m_bits; }

    ///
    /// Different operator.
    ///
    /// @param[in]  other  The other bitfield.
    ///
    /// @return     true if other is equal to current.
    ///
    constexpr bool operator!=(const BitField& other) const { return m_bits != other.m_bits; }

    ///
    /// Bitwise 'or' operator.
    ///
    /// @param[in]  other  The other bitfield.
    ///
    /// @return     The result of the bitwise 'or'
    ///
    constexpr BitField operator|(const BitField& other) const
    {
        return BitField(m_bits | other.m_bits);
    }

    ///
    /// Bitwise 'and' operator.
    ///
    /// @param[in]  other  The other bitfield.
    ///
    /// @return     The result of the bitwise 'and'.
    ///
    constexpr BitField operator&(const BitField& other) const
    {
        return BitField(m_bits & other.m_bits);
    }

    ///
    /// Bitwise 'exclusive or' operator.
    ///
    /// @param[in]  other  The other bitfield.
    ///
    /// @return     The result of the bitwise 'exclusive or'.
    ///
    constexpr BitField operator^(const BitField& other) const
    {
        return BitField(m_bits ^ other.m_bits);
    }

    ///
    /// Bitwise 'one's complement' operator.
    ///
    /// @return     The result of the bitwise 'one's complement'.
    ///
    constexpr BitField operator~() const { return BitField(~m_bits); }

private:
    static_assert(std::is_enum_v<EnumType>);

    UnderlyingType m_bits = UnderlyingType(0);
};

/// @}

} // namespace lagrange

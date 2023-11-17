/*
 * Copyright 2019 Adobe. All rights reserved.
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

#include <lagrange/Logger.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/warning.h>

#include <limits>

namespace lagrange {

/// @addtogroup group-utils-misc
/// @{

///
/// Perform safe cast from `SourceType` to `TargetType`, where "safe" means:
///
///   - Type compatibility.
///   - No over/under flow for numerical types.
///   - No sign change caused by casting.
///   - No large numerical error for floating point casts.
///
/// Example usage:
///
/// @code
/// int x = lagrange::safe_cast<int>(-1.0);       // good.
/// int x = lagrange::safe_cast<int>("-1.0");     // fail because incompatible types.
/// size_t x = lagrange::safe_cast<size_t>(-1.0); // fail because sign change.
/// int x = lagrange::safe_cast<int>(-1.5);       // fail because truncation error.
/// @endcode
///
/// @param[in]  value       Value to cast.
///
/// @tparam     TargetType  Target scalar type.
/// @tparam     SourceType  Source scalar type.
///
/// @return     Casted value.
///
template <typename TargetType, typename SourceType>
constexpr auto safe_cast(SourceType value)
    -> std::enable_if_t<!std::is_same<SourceType, TargetType>::value, TargetType>
{
    if constexpr (std::is_integral_v<TargetType> && std::is_floating_point_v<SourceType>) {
        SourceType float_max = std::nextafter(
            static_cast<SourceType>(std::numeric_limits<TargetType>::max()),
            SourceType(0));
        SourceType float_min = std::nextafter(
            static_cast<SourceType>(std::numeric_limits<TargetType>::min()),
            SourceType(0));
        if (value > float_max || value < float_min) {
            logger().error("Casting failed: float cast overflow for float {}", value);
            throw BadCastError();
        }
    }

    TargetType value_2 = static_cast<TargetType>(value);

    if constexpr (std::is_integral_v<SourceType> && std::is_floating_point_v<TargetType>) {
        TargetType float_max = std::nextafter(
            static_cast<TargetType>(std::numeric_limits<SourceType>::max()),
            TargetType(0));
        TargetType float_min = std::nextafter(
            static_cast<TargetType>(std::numeric_limits<SourceType>::min()),
            TargetType(0));
        if (value_2 > float_max || value_2 < float_min) {
            logger().error("Casting failed: float cast overflow for integer {}", value);
            throw BadCastError();
        }
    }

    SourceType value_3 = static_cast<SourceType>(value_2);

    if ((value_2 >= 0) != (value >= 0)) {
        // Sign changed. Not good.
        logger().error("Casting failed: from {} to {} causes a sign change...", value, value_2);
        throw BadCastError();
    } else if (value_3 == value) {
        // Lossless cast. :D
        return value_2;
    } else {
        // Lossy cast... Check for casting error.
        constexpr SourceType EPS =
            static_cast<SourceType>(std::numeric_limits<TargetType>::epsilon());

        // Generates warning C4146: "unary minus operator applied to unsigned type, result still
        // unsigned" this cannot happen, as we check for unsigned types above.
        LA_DISABLE_WARNING_BEGIN
        LA_DISABLE_WARNING_MSVC(4146)
        const SourceType value_abs = value_3 > 0 ? value_3 : -value_3;
        LA_DISABLE_WARNING_END

        const SourceType scaled_eps = value_abs >= SourceType(1) ? EPS * value_abs : EPS;
        if (value_3 > value && value_3 < value + scaled_eps) {
            return value_2;
        } else if (value_3 < value && value_3 + scaled_eps > value) {
            return value_2;
        } else {
            // Cast is likely not valid...
            logger().error(
                "Casting failed: from {} to {} will incur error ({}) larger than {}",
                value,
                value_2,
                value - static_cast<SourceType>(value_2),
                scaled_eps);
            throw BadCastError();
        }
    }
}

///
/// Safe cast specialization for TargetType == SourceType
///
/// @param[in]  value  Value to cast.
///
/// @tparam     T      Scalar type.
///
/// @return     Casted value.
///
template <typename T>
constexpr T safe_cast(T value)
{
    return value;
}

///
/// Safe cast specialization for bool.
///
/// @param[in]  value       Value to cast.
///
/// @tparam     TargetType  Target scalar type.
///
/// @return     Casted value.
///
template <typename TargetType>
constexpr TargetType safe_cast(bool value)
{
    // do we really want to allow this?
    return static_cast<TargetType>(value);
}

///
/// Casting an enum to scalar and vice versa. These are only to be used for assigning enums as
/// (mesh) attributes, or to be used for reading back enums that were saved as mesh attributes.
///
/// @param[in]  u     Value to cast.
///
/// @tparam     T     Target scalar/enum type.
/// @tparam     U     Source scalar/enum type.
///
/// @return     Casted value.
///
template <typename T, typename U>
constexpr T safe_cast_enum(const U u)
{
    static_assert(
        std::is_enum_v<T> || std::is_enum_v<U>,
        "At least one of the types should be an enum");
    static_assert(
        (std::is_enum_v<T> && std::is_enum_v<U>) == std::is_same_v<T, U>,
        "Casting one enum to another is prohibited");

    using underlying_t = std::underlying_type_t<std::conditional_t<std::is_enum_v<T>, T, U>>;
    using enum_t = std::conditional_t<std::is_enum_v<T>, T, U>;
    using other_t = std::conditional_t<std::is_enum_v<T>, U, T>;

    if constexpr (std::is_enum_v<T>) {
        return static_cast<enum_t>(safe_cast<underlying_t>(u));
    } else {
        return safe_cast<other_t>(static_cast<underlying_t>(u));
    }
}

/// @}

} // namespace lagrange

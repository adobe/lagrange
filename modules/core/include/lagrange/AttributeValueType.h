/*
 * Copyright 2024 Adobe. All rights reserved.
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

#include <lagrange/AttributeTypes.h>

namespace lagrange {

/// @addtogroup group-surfacemesh-attr
/// @{

///
/// Enum describing at runtime the value type of an attribute. This can be accessed from the base
/// attribute class and enables safe downcasting without global RTTI.
///
enum class AttributeValueType : uint8_t {
#define LA_X_attribute_value_type_enum(_, ValueType) e_##ValueType,
    LA_ATTRIBUTE_X(attribute_value_type_enum, 0)
#undef LA_X_attribute_value_type_enum
};

///
/// Creates an enum describing an attribute value type.
///
/// @tparam     ValueType  Value type of the attribute to convert to enum.
///
/// @return     Enum describing the input value type.
///
template <typename ValueType>
constexpr AttributeValueType make_attribute_value_type();

#define LA_X_attribute_make_value_type(_, ValueType)                    \
    template <>                                                         \
    constexpr AttributeValueType make_attribute_value_type<ValueType>() \
    {                                                                   \
        return AttributeValueType::e_##ValueType;                       \
    }
LA_ATTRIBUTE_X(attribute_make_value_type, 0)
#undef LA_X_attribute_make_value_type

/// @}

} // namespace lagrange

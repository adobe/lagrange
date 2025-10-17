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
#include <lagrange/internal/attribute_string_utils.h>

#include <lagrange/AttributeTypes.h>
#include <lagrange/utils/assert.h>

#define LA_ENUM_CASE(e, x) \
    case e::x: return #x

namespace lagrange::internal {

std::string_view to_string(AttributeElement element)
{
    switch (element) {
        LA_ENUM_CASE(AttributeElement, Vertex);
        LA_ENUM_CASE(AttributeElement, Facet);
        LA_ENUM_CASE(AttributeElement, Edge);
        LA_ENUM_CASE(AttributeElement, Corner);
        LA_ENUM_CASE(AttributeElement, Value);
        LA_ENUM_CASE(AttributeElement, Indexed);
    default: la_debug_assert(false, "Unsupported enum type"); return "";
    }
}

std::string to_string(BitField<AttributeElement> element)
{
    std::string ret;
    if (element.test_any(AttributeElement::Vertex)) ret += "Vertex;";
    if (element.test_any(AttributeElement::Facet)) ret += "Facet;";
    if (element.test_any(AttributeElement::Edge)) ret += "Edge;";
    if (element.test_any(AttributeElement::Corner)) ret += "Corner;";
    if (element.test_any(AttributeElement::Value)) ret += "Value;";
    if (element.test_any(AttributeElement::Indexed)) ret += "Indexed;";
    return ret;
}

std::string_view to_string(AttributeUsage usage)
{
    switch (usage) {
        LA_ENUM_CASE(AttributeUsage, Vector);
        LA_ENUM_CASE(AttributeUsage, Scalar);
        LA_ENUM_CASE(AttributeUsage, Normal);
        LA_ENUM_CASE(AttributeUsage, Position);
        LA_ENUM_CASE(AttributeUsage, Tangent);
        LA_ENUM_CASE(AttributeUsage, Bitangent);
        LA_ENUM_CASE(AttributeUsage, Color);
        LA_ENUM_CASE(AttributeUsage, UV);
        LA_ENUM_CASE(AttributeUsage, VertexIndex);
        LA_ENUM_CASE(AttributeUsage, FacetIndex);
        LA_ENUM_CASE(AttributeUsage, CornerIndex);
        LA_ENUM_CASE(AttributeUsage, EdgeIndex);
    default: la_debug_assert(false, "Unsupported enum type"); return "";
    }
}

#define LA_X_type_name(_, ValueType)                                                    \
    template <>                                                                         \
    LA_CORE_API std::string_view value_type_name(const lagrange::Attribute<ValueType>&) \
    {                                                                                   \
        return #ValueType;                                                              \
    }                                                                                   \
    template <>                                                                         \
    LA_CORE_API std::string_view value_type_name<ValueType>()                           \
    {                                                                                   \
        return #ValueType;                                                              \
    }

LA_ATTRIBUTE_X(type_name, 0)

} // namespace lagrange::internal

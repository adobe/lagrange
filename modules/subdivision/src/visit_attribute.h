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

#include <lagrange/Attribute.h>
#include <lagrange/AttributeTypes.h>
#include <lagrange/AttributeValueType.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/SurfaceMesh.h>

namespace lagrange::subdivision {

///
/// Apply a function to a mesh attribute.
///
/// @param[in]  mesh    Input mesh.
/// @param[in]  id      Attribute id to apply the function to.
/// @param      func    Function to apply.
///
/// @tparam     Func    Function type.
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
template <typename Func, typename Scalar, typename Index>
void visit_attribute(const SurfaceMesh<Scalar, Index>& mesh, AttributeId id, Func&& func)
{
    const auto& attr = mesh.get_attribute_base(id);
    auto type = attr.get_value_type();
    bool is_indexed = (attr.get_element_type() == AttributeElement::Indexed);
    switch (type) {
#define LA_X_visit(_, ValueType)                                                \
    case make_attribute_value_type<ValueType>(): {                              \
        if (is_indexed) {                                                       \
            func(static_cast<const IndexedAttribute<ValueType, Index>&>(attr)); \
        } else {                                                                \
            func(static_cast<const Attribute<ValueType>&>(attr));               \
        }                                                                       \
        break;                                                                  \
    }
        LA_ATTRIBUTE_X(visit, 0)
#undef LA_X_visit
    }
}

} // namespace lagrange::subdivision

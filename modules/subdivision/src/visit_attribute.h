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

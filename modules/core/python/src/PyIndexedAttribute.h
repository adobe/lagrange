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

#include "PyAttribute.h"

#include <lagrange/Attribute.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/internal/weak_ptr.h>
#include <lagrange/utils/Error.h>

#include <memory>

namespace lagrange::python {

class PyIndexedAttribute
{
public:
    PyIndexedAttribute(internal::weak_ptr<AttributeBase> ptr)
        : m_attr(ptr)
    {}

public:
    internal::shared_ptr<AttributeBase> operator->() { return ptr(); }

    internal::shared_ptr<AttributeBase> ptr()
    {
        auto attr = m_attr.lock();
        if (attr == nullptr) throw Error("Indexed attribute is no longer valid!");
        return attr;
    }

    template <typename CallBack>
    auto process(CallBack&& cb)
    {
        auto attr = m_attr.lock();
        if (attr == nullptr) throw Error("Indexed attribute is no longer valid!");
        if (attr->get_element_type() != Indexed)
            throw Error("Attribute is not an indexed attribute");

#define LA_X_process(Index, ValueType)                                                     \
    {                                                                                      \
        auto indexed_attr = dynamic_cast<IndexedAttribute<ValueType, Index>*>(attr.get()); \
        if (indexed_attr != nullptr) {                                                     \
            return cb(*indexed_attr);                                                      \
        }                                                                                  \
    }
#define LA_X_process_index(_, Index) LA_ATTRIBUTE_X(process, Index)
        LA_SURFACE_MESH_INDEX_X(process_index, 0)
#undef LA_X_process_index
#undef LA_X_process
        throw Error("Cannot process indexed attribute with unsupported types!");
    }

    PyAttribute values()
    {
        return process([&](auto& indexed_attr) {
            auto attr = m_attr.lock();
            la_debug_assert(attr != nullptr);
            auto& value_attr = indexed_attr.values();
            internal::shared_ptr<AttributeBase> alias_ptr(attr, &value_attr);
            return PyAttribute(alias_ptr);
        });
    }

    PyAttribute indices()
    {
        return process([&](auto& indexed_attr) {
            auto attr = m_attr.lock();
            la_debug_assert(attr != nullptr);
            auto& index_attr = indexed_attr.indices();
            internal::shared_ptr<AttributeBase> alias_ptr(attr, &index_attr);
            return PyAttribute(alias_ptr);
        });
    }

private:
    ::lagrange::internal::weak_ptr<AttributeBase> m_attr;
};

} // namespace lagrange::python

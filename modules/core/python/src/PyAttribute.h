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
#include <lagrange/AttributeTypes.h>
#include <lagrange/utils/Error.h>
#include <lagrange/internal/weak_ptr.h>

#include <memory>

namespace lagrange::python {

class PyAttribute
{
public:
    PyAttribute(internal::weak_ptr<AttributeBase> ptr)
        : m_attr(ptr)
    {}

public:
    internal::shared_ptr<AttributeBase> operator->() { return ptr(); }

    internal::shared_ptr<AttributeBase> ptr()
    {
        auto attr = m_attr.lock();
        if (attr == nullptr) throw Error("Attribute is no longer valid!");
        return attr;
    }

    template <typename ValueType>
    internal::shared_ptr<Attribute<ValueType>> ptr()
    {
        auto attr = m_attr.lock();
        if (attr == nullptr) throw Error("Attribute is no longer valid!");
        return internal::shared_ptr<Attribute<ValueType>>(attr, dynamic_cast<Attribute<ValueType>*>(attr.get()));
    }

    template <typename CallBack>
    auto process(CallBack&& cb)
    {
#define LA_X_process(_, ValueType)    \
    {                                 \
        auto attr = ptr<ValueType>(); \
        if (attr != nullptr) {        \
            return cb(*attr);         \
        }                             \
    }
        LA_ATTRIBUTE_X(process, 0)
#undef LA_X_process
        throw Error("Cannot process attribute with unsupported type!");
    }

private:
    ::lagrange::internal::weak_ptr<AttributeBase> m_attr;
};

} // namespace lagrange::python

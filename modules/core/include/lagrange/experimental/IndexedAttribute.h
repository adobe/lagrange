/*
 * Copyright 2020 Adobe. All rights reserved.
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

#include <lagrange/common.h>
#include <lagrange/experimental/Attribute.h>
#include <lagrange/utils/warning.h>

namespace lagrange {
namespace experimental {

class IndexedAttribute
{
public:
    IndexedAttribute() = default;

    template <typename ValueArrayDerived, typename IndexArrayDerived>
    IndexedAttribute(ValueArrayDerived&& values, IndexArrayDerived&& indices)
        : m_values(std::forward<ValueArrayDerived>(values))
        , m_indices(std::forward<IndexArrayDerived>(indices))
    {}

public:
    std::shared_ptr<const ArrayBase> get_values() const { return m_values.get(); }
    std::shared_ptr<ArrayBase> get_values() { return m_values.get(); }

    template <typename Derived>
    decltype(auto) get_values() const
    {
        return m_values.template get<Derived>();
    }

    template <typename Derived>
    decltype(auto) get_values()
    {
        return m_values.template get<Derived>();
    }

    template <typename Derived>
    decltype(auto) view_values() const
    {
        return m_values.template view<Derived>();
    }

    template <typename Derived>
    decltype(auto) view_values()
    {
        return m_values.template view<Derived>();
    }

    template <typename Derived>
    void set_values(Derived&& values)
    {
        m_values.set(std::forward<Derived>(values));
    }

    std::shared_ptr<const ArrayBase> get_indices() const { return m_indices.get(); }

    std::shared_ptr<ArrayBase> get_indices() { return m_indices.get(); }

    template <typename Derived>
    decltype(auto) get_indices() const
    {
        return m_indices.template get<Derived>();
    }

    template <typename Derived>
    decltype(auto) get_indices()
    {
        return m_indices.template get<Derived>();
    }

    template <typename Derived>
    decltype(auto) view_indices() const
    {
        return m_indices.template view<Derived>();
    }

    template <typename Derived>
    decltype(auto) view_indices()
    {
        return m_indices.template view<Derived>();
    }

    template <typename Derived>
    void set_indices(Derived&& indices)
    {
        m_indices.set(std::forward<Derived>(indices));
    }

    template <typename Archive>
    void serialize_impl(Archive& ar)
    {
        LA_IGNORE_SHADOW_WARNING_BEGIN
        enum { VALUES = 0, INDICES = 1 };
        ar.object([&](auto& ar) {
            ar("values", VALUES) & m_values;
            ar("indices", INDICES) & m_indices;
        });
        LA_IGNORE_SHADOW_WARNING_END
    }

private:
    Attribute m_values;
    Attribute m_indices;
};

template <typename Archive>
void serialize(::lagrange::experimental::IndexedAttribute& attribute, Archive& ar)
{
    attribute.serialize_impl(ar);
}

} // namespace experimental
} // namespace lagrange

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

#include <map>
#include <string>

#include <lagrange/Logger.h>
#include <lagrange/common.h>
#include <lagrange/experimental/IndexedAttribute.h>
#include <lagrange/utils/assert.h>

namespace lagrange {
namespace experimental {

class IndexedAttributeManager
{
public:
    std::vector<std::string> get_names() const
    {
        std::vector<std::string> names;
        for (const auto& itr : m_data) {
            names.push_back(itr.first);
        }
        return names;
    }

    size_t get_size() const { return m_data.size(); }

    bool has(const std::string& name) const { return m_data.find(name) != m_data.end(); }

    void add(const std::string& name)
    {
        m_data.emplace(name, std::make_unique<IndexedAttribute>());
    }

    template <typename ValueDerived, typename IndexDerived>
    void add(const std::string& name, ValueDerived&& values, IndexDerived&& indices)
    {
        auto attr = std::make_unique<IndexedAttribute>(
            std::forward<ValueDerived>(values),
            std::forward<IndexDerived>(indices));
        m_data.emplace(name, std::move(attr));
    }

    template <typename ValueDerived, typename IndexDerived>
    void set(const std::string& name, ValueDerived&& values, IndexDerived&& indices)
    {
        auto itr = m_data.find(name);
        la_runtime_assert(itr != m_data.end(), "Indexed attribute " + name + " does not exist.");
        if (itr->second == nullptr) {
            itr->second = std::make_unique<IndexedAttribute>(
                std::forward<ValueDerived>(values),
                std::forward<IndexDerived>(indices));
        } else {
            itr->second->set_values(std::forward<ValueDerived>(values));
            itr->second->set_indices(std::forward<IndexDerived>(indices));
        }
    }

    IndexedAttribute* get(const std::string& name)
    {
        auto itr = m_data.find(name);
        la_runtime_assert(itr != m_data.end(), "Indexed attribute " + name + " does not exist.");
        return itr->second.get();
    }

    const IndexedAttribute* get(const std::string& name) const
    {
        auto itr = m_data.find(name);
        la_runtime_assert(itr != m_data.end(), "Indexed attribute " + name + " does not exist.");
        return itr->second.get();
    }

    template <typename Derived>
    decltype(auto) get_values(const std::string& name)
    {
        auto ptr = get(name);
        la_runtime_assert(ptr != nullptr, "Attribute " + name + " is null.");
        return ptr->get_values<Derived>();
    }

    template <typename Derived>
    decltype(auto) get_values(const std::string& name) const
    {
        const auto ptr = get(name);
        la_runtime_assert(ptr != nullptr, "Attribute " + name + " is null.");
        return ptr->get_values<Derived>();
    }

    template <typename Derived>
    decltype(auto) get_indices(const std::string& name)
    {
        auto ptr = get(name);
        la_runtime_assert(ptr != nullptr, "Attribute " + name + " is null.");
        return ptr->get_indices<Derived>();
    }

    template <typename Derived>
    decltype(auto) get_indices(const std::string& name) const
    {
        const auto ptr = get(name);
        la_runtime_assert(ptr != nullptr, "Attribute " + name + " is null.");
        return ptr->get_indices<Derived>();
    }

    template <typename Derived>
    decltype(auto) view_values(const std::string& name)
    {
        auto ptr = get(name);
        la_runtime_assert(ptr != nullptr, "Attribute " + name + " is null.");
        return ptr->view_values<Derived>();
    }

    template <typename Derived>
    decltype(auto) view_values(const std::string& name) const
    {
        const auto ptr = get(name);
        la_runtime_assert(ptr != nullptr, "Attribute " + name + " is null.");
        return ptr->view_values<Derived>();
    }

    template <typename Derived>
    decltype(auto) view_indices(const std::string& name)
    {
        auto ptr = get(name);
        la_runtime_assert(ptr != nullptr, "Attribute " + name + " is null.");
        return ptr->view_indices<Derived>();
    }

    template <typename Derived>
    decltype(auto) view_indices(const std::string& name) const
    {
        const auto ptr = get(name);
        la_runtime_assert(ptr != nullptr, "Attribute " + name + " is null.");
        return ptr->view_indices<Derived>();
    }

    template <typename ValueDerived, typename IndexDerived>
    void import_data(const std::string& name, ValueDerived&& values, IndexDerived&& indices)
    {
        set(name, std::move(values.derived()), std::move(indices.derived()));
    }

    template <typename ValueDerived, typename IndexDerived>
    void export_data(
        const std::string& name,
        Eigen::PlainObjectBase<ValueDerived>& values,
        Eigen::PlainObjectBase<IndexDerived>& indices)
    {
        auto attr = get(name);
        la_runtime_assert(attr != nullptr, "Attribute " + name + " is null.");
        auto value_array = attr->get_values();
        auto index_array = attr->get_indices();
        la_runtime_assert(value_array != nullptr);
        la_runtime_assert(index_array != nullptr);

#ifndef NDEBUG
        bool validate = true;
        void* value_ptr = value_array->data();
        void* index_ptr = index_array->data();
#endif
        try {
            ValueDerived& value_matrix = value_array->template get<ValueDerived>();
            IndexDerived& index_matrix = index_array->template get<IndexDerived>();
            values.swap(value_matrix);
            indices.swap(index_matrix);
        } catch (std::runtime_error& e) {
            // It seems ValueDerived or IndexDerived are not exact match to the
            // Eigen type used for creating the attribute.  A copy is necessary.
            logger().warn("Export cannot be done without coping: {}", e.what());
            values = value_array->view<ValueDerived>();
            indices = index_array->view<IndexDerived>();
#ifndef NDEBUG
            validate = false;
#endif
        }

#ifndef NDEBUG
        if (validate) {
            la_runtime_assert(value_ptr == values.data(), "Export values fall back to copying.");
            la_runtime_assert(index_ptr == indices.data(), "Export indices fall back to copying.");
        }
#endif
    }

    void remove(const std::string& name)
    {
        auto itr = m_data.find(name);
        la_runtime_assert(itr != m_data.end(), "Attribute " + name + " does not exist.");
        m_data.erase(itr);
    }

    template <typename Archive>
    void serialize_impl(Archive& ar)
    {
        LA_IGNORE_SHADOW_WARNING_BEGIN
        enum { DATA = 0 };
        ar.object([&](auto& ar) { ar("data", DATA) & m_data; });
        LA_IGNORE_SHADOW_WARNING_END
    }

private:
    std::map<std::string, std::unique_ptr<IndexedAttribute>> m_data;
};

LA_IGNORE_SHADOW_WARNING_BEGIN

template <typename Archive>
void serialize(std::pair<std::string, IndexedAttribute>& entry, Archive& ar)
{
    enum { KEY = 0, VALUE = 1 };

    ar.object([&](auto& ar) {
        ar("key", KEY) & entry.first;
        ar("value", VALUE) & entry.second;
    });
}

template <typename Archive>
void serialize(std::map<std::string, std::unique_ptr<IndexedAttribute>>& attrs, Archive& ar)
{
    std::vector<std::pair<std::string, ::lagrange::experimental::IndexedAttribute>> data;
    if (!ar.is_input()) {
        for (auto& itr : attrs) {
            data.emplace_back(itr.first, *itr.second);
        }
    }

    serialize(data, ar);

    if (ar.is_input()) {
        attrs.clear();
        for (const auto& itr : data) {
            attrs[itr.first] = std::make_unique<IndexedAttribute>(itr.second);
        }
    }
}

template <typename Archive>
void serialize(IndexedAttributeManager& attributes, Archive& ar)
{
    attributes.serialize_impl(ar);
}

LA_IGNORE_SHADOW_WARNING_END

} // namespace experimental
} // namespace lagrange

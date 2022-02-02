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
#include <lagrange/experimental/Attribute.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/warning.h>

namespace lagrange {
namespace experimental {

class AttributeManager
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

    size_t get_size() const {
        return m_data.size();
    }

    bool has(const std::string& name) const { return m_data.find(name) != m_data.end(); }

    void add(const std::string& name) { m_data.emplace(name, std::make_unique<Attribute>()); }

    template <typename Derived>
    void add(const std::string& name, Derived&& values)
    {
        auto attr = std::make_unique<Attribute>(std::forward<Derived>(values));
        m_data.emplace(name, std::move(attr));
    }

    template <typename Derived>
    void set(const std::string& name, Derived&& values)
    {
        auto itr = m_data.find(name);
        la_runtime_assert(itr != m_data.end(), "Attribute " + name + " does not exist.");
        if (itr->second == nullptr) {
            auto attr = std::make_unique<Attribute>();
            attr->set(std::forward<Derived>(values));
            itr->second = std::move(attr);
        } else {
            itr->second->set(std::forward<Derived>(values));
        }
    }

    Attribute* get(const std::string& name)
    {
        auto itr = m_data.find(name);
        la_runtime_assert(itr != m_data.end(), "Attribute " + name + " does not exist.");
        return itr->second.get();
    }

    const Attribute* get(const std::string& name) const
    {
        auto itr = m_data.find(name);
        la_runtime_assert(itr != m_data.end(), "Attribute " + name + " does not exist.");
        return itr->second.get();
    }

    template <typename Derived>
    decltype(auto) get(const std::string& name)
    {
        auto ptr = get(name);
        la_runtime_assert(ptr != nullptr, "Attribute " + name + " is null.");
        return ptr->get<Derived>();
    }

    template <typename Derived>
    decltype(auto) get(const std::string& name) const
    {
        const auto ptr = get(name);
        la_runtime_assert(ptr != nullptr, "Attribute " + name + " is null.");
        return ptr->get<Derived>();
    }

    template <typename Derived>
    decltype(auto) view(const std::string& name)
    {
        auto ptr = get(name);
        la_runtime_assert(ptr != nullptr, "Attribute " + name + " is null.");
        return ptr->view<Derived>();
    }

    template <typename Derived>
    decltype(auto) view(const std::string& name) const
    {
        const auto ptr = get(name);
        la_runtime_assert(ptr != nullptr, "Attribute " + name + " is null.");
        return ptr->view<Derived>();
    }

    template <typename Derived>
    void import_data(const std::string& name, Derived&& values)
    {
        set(name, std::move(values.derived()));
    }

    template <typename Derived>
    void export_data(const std::string& name, Eigen::PlainObjectBase<Derived>& values)
    {
        auto attr = get(name);
        la_runtime_assert(attr != nullptr, "Attribute " + name + " is null.");
        auto value_array = attr->get();

#ifndef NDEBUG
        bool validate = true;
        void* value_ptr = value_array->data();
#endif
        try {
            Derived& value_matrix = value_array->template get<Derived>();
            values.swap(value_matrix);
        } catch (std::runtime_error& e) {
            // It seems Derived is not an exact match to the
            // Eigen type used for creating the attribute.  A copy is necessary.
            logger().warn("Export cannot be done without coping: {}", e.what());
            values = value_array->view<Derived>();
#ifndef NDEBUG
            validate = false;
#endif
        }

#ifndef NDEBUG
        if (validate) {
            la_runtime_assert(value_ptr == values.data(), "Export have to fall back to copying.");
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
        ar.object([&](auto& ar) {
            ar("data", DATA) & m_data;
        });
        LA_IGNORE_SHADOW_WARNING_END
    }

private:
    std::map<std::string, std::unique_ptr<Attribute>> m_data;
};

LA_IGNORE_SHADOW_WARNING_BEGIN

template <typename Archive>
void serialize(std::pair<std::string, Attribute>& entry, Archive& ar)
{
    enum { KEY = 0, VALUE = 1 };

    ar.object([&](auto& ar) {
        ar("key", KEY) & entry.first;
        ar("value", VALUE) & entry.second;
    });
}

template <typename Archive>
void serialize(
    std::map<std::string, std::unique_ptr<Attribute>>& attrs,
    Archive& ar)
{
    std::vector<std::pair<std::string, Attribute>> data;
    if (!ar.is_input()) {
        for (auto& itr : attrs) {
            data.emplace_back(itr.first, *itr.second);
        }
    }

    serialize(data, ar);

    if (ar.is_input()) {
        attrs.clear();
        for (const auto& itr : data) {
            attrs[itr.first] = std::make_unique<Attribute>(itr.second);
        }
    }
}

template <typename Archive>
void serialize(AttributeManager& attributes, Archive& ar)
{
    attributes.serialize_impl(ar);
}

LA_IGNORE_SHADOW_WARNING_END

} // namespace experimental
} // namespace lagrange

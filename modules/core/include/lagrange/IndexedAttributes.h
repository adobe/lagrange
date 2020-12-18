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

#include <lagrange/GenuineMeshGeometry.h>
#include <lagrange/utils/la_assert.h>

namespace lagrange {

template <typename _AttributeArray, typename _IndexArray>
class IndexedAttributes
{
public:
    using AttributeArray = _AttributeArray;
    using IndexArray = _IndexArray;
    using AttributeData = GenuineMeshGeometry<AttributeArray, IndexArray>;
    using Scalar = typename AttributeArray::Scalar;
    using Index = typename IndexArray::Scalar;

public:
    std::vector<std::string> get_attribute_names() const
    {
        std::vector<std::string> names;
        for (const auto& itr : m_data) {
            names.push_back(itr.first);
        }
        return names;
    }

    bool has_attribute(const std::string& name) const { return m_data.find(name) != m_data.end(); }

    void add_attribute(const std::string& name)
    {
        m_data.emplace(name, std::make_shared<AttributeData>());
    }

    template <typename ValueDerived, typename IndexDerived>
    void add_attribute(
        const std::string& name,
        const Eigen::PlainObjectBase<ValueDerived>& values,
        const Eigen::PlainObjectBase<IndexDerived>& indices)
    {
        auto data =
            std::make_shared<GenuineMeshGeometry<AttributeArray, IndexArray>>(values, indices);
        m_data.emplace(name, std::move(data));
    }

    template <typename ValueDerived, typename IndexDerived>
    void set_attribute(
        const std::string& name,
        const Eigen::PlainObjectBase<ValueDerived>& values,
        const Eigen::PlainObjectBase<IndexDerived>& indices)
    {
        auto itr = m_data.find(name);
        LA_ASSERT(itr != m_data.end(), "Attribute " + name + " does not exist");
        itr->second =
            std::make_shared<GenuineMeshGeometry<AttributeArray, IndexArray>>(values, indices);
    }

    const AttributeData& get_attribute(const std::string& name) const
    {
        auto itr = m_data.find(name);
        LA_ASSERT(itr != m_data.end(), "Attribute " + name + " does not exist");
        return *itr->second;
    }

    AttributeData& get_attribute(const std::string& name)
    {
        auto itr = m_data.find(name);
        LA_ASSERT(itr != m_data.end(), "Attribute " + name + " does not exist");
        return *itr->second;
    }

    const AttributeArray& get_attribute_values(const std::string& name) const
    {
        return get_attribute(name).get_vertices();
    }

    AttributeArray& get_attribute_values(const std::string& name)
    {
        return get_attribute(name).get_vertices();
    }

    const IndexArray& get_attribute_indices(const std::string& name) const
    {
        return get_attribute(name).get_facets();
    }

    IndexArray& get_attribute_indices(const std::string& name)
    {
        return get_attribute(name).get_facets();
    }

    void remove_attribute(const std::string& name)
    {
        auto itr = m_data.find(name);
        LA_ASSERT(itr != m_data.end(), "Attribute " + name + " does not exist");
        m_data.erase(itr);
    }

    template <typename ValueDerived, typename IndexDerived>
    void import_attribute(
        const std::string& name,
        Eigen::PlainObjectBase<ValueDerived>& values,
        Eigen::PlainObjectBase<IndexDerived>& indices)
    {
        auto itr = m_data.find(name);
        LA_ASSERT(itr != m_data.end(), "Attribute " + name + " does not exist");
        itr->second->import_vertices(values);
        itr->second->import_facets(indices);
    }

    template <typename ValueDerived, typename IndexDerived>
    void export_attribute(
        const std::string& name,
        Eigen::PlainObjectBase<ValueDerived>& values,
        Eigen::PlainObjectBase<IndexDerived>& indices)
    {
        auto itr = m_data.find(name);
        LA_ASSERT(itr != m_data.end(), "Attribute " + name + " does not exist");
        itr->second->export_vertices(values);
        itr->second->export_facets(indices);
    }

    decltype(auto) get_attribute_as_mesh(const std::string& name)
    {
        auto itr = m_data.find(name);
        LA_ASSERT(itr != m_data.end(), "Attribute " + name + " does not exist");
        using MeshType = Mesh<AttributeArray, IndexArray>;
        return std::make_unique<MeshType>(itr->second);
    }

private:
    std::map<std::string, std::shared_ptr<AttributeData>> m_data;
};

} // namespace lagrange

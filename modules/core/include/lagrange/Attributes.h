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

#include <Eigen/Core>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <lagrange/common.h>
#include <lagrange/utils/warning.h>

namespace lagrange {

/// Legacy attribute class.
template <typename _AttributeArray>
class Attributes
{
public:
    using AttributeArray = _AttributeArray;
    using Scalar = typename AttributeArray::Scalar;

public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    std::vector<std::string> get_attribute_names() const
    {
        std::vector<std::string> names;
        for (const auto& itr : m_data) {
            names.push_back(itr.first);
        }
        return names;
    }

    bool has_attribute(const std::string& name) const { return m_data.find(name) != m_data.end(); }

    void add_attribute(const std::string& name) { m_data.insert({name, AttributeArray()}); }

    template <typename Derived>
    void add_attribute(const std::string& name, const Eigen::PlainObjectBase<Derived>& value)
    {
        m_data.insert({name, value});
    }

    template <typename Derived>
    void set_attribute(const std::string& name, const Eigen::PlainObjectBase<Derived>& value)
    {
        auto itr = m_data.find(name);
        if (itr == m_data.end()) {
            std::stringstream err_msg;
            err_msg << "Attributes::set_attribute() failed: Attribute \"" << name
                    << "\" does not exists.";
            throw std::runtime_error(err_msg.str());
        }
        itr->second = value;
    }

    const AttributeArray& get_attribute(const std::string& name) const
    {
        auto itr = m_data.find(name);
        if (itr == m_data.end()) {
            std::stringstream err_msg;
            err_msg << "Attributes::get_attribute() failed: Attribute \"" << name
                    << "\" does not exists.";
            throw std::runtime_error(err_msg.str());
        }
        return itr->second;
    }

    void remove_attribute(const std::string& name)
    {
        auto itr = m_data.find(name);
        if (itr == m_data.end()) {
            std::stringstream err_msg;
            err_msg << "Attributes::remove_attribute() failed: Attribute \"" << name
                    << "\" does not exists.";
            throw std::runtime_error(err_msg.str());
        }
        m_data.erase(itr);
    }

    template <typename Derived>
    void import_attribute(const std::string& name, Eigen::PlainObjectBase<Derived>& attr)
    {
        auto itr = m_data.find(name);
        if (itr == m_data.end()) {
            std::stringstream err_msg;
            err_msg << "Attributes::import_attribute() failed: Attribute \"" << name
                    << "\" does not exists.";
            throw std::runtime_error(err_msg.str());
        }
        move_data(attr, itr->second);
    }

    template <typename Derived>
    void export_attribute(const std::string& name, Eigen::PlainObjectBase<Derived>& attr)
    {
        auto itr = m_data.find(name);
        if (itr == m_data.end()) {
            std::stringstream err_msg;
            err_msg << "Attributes::export_attribute() failed: Attribute \"" << name
                    << "\" does not exists.";
            throw std::runtime_error(err_msg.str());
        }
        move_data(itr->second, attr);
    }

    template <typename Archive>
    void serialize(Archive& ar)
    {
        enum { //
            DATA = 0,
        };
        LA_IGNORE_SHADOW_WARNING_BEGIN
        ar.object([&](auto& ar) { //
            ar("data", DATA) & m_data;
        });
        LA_IGNORE_SHADOW_WARNING_END
    }

protected:
    std::map<std::string, AttributeArray> m_data;
};

template <typename AttributeArray, typename Archive>
void serialize(Attributes<AttributeArray>& attributes, Archive& ar)
{
    attributes.serialize(ar);
} // end of serialize()

} // namespace lagrange

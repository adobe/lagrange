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

#include <any>
#include <memory>
#include <vector>

namespace lagrange {

namespace ui {

class BaseResourceData
{
public:
    BaseResourceData() = default;
    virtual ~BaseResourceData() = default;

    template <typename Params>
    BaseResourceData(Params params)
        : m_params(std::move(params))
    {}

    /// Sets the dirty flag to value
    void set_dirty(bool value) { m_dirty = value; }

    bool is_dirty() const { return m_dirty; }

    /// Adds resource as a dependency of this data
    void add_dependency(const std::shared_ptr<BaseResourceData>& resource_data)
    {
        m_dependencies.push_back(resource_data);
    }

    const std::vector<std::shared_ptr<BaseResourceData>>& dependencies() const
    {
        return m_dependencies;
    }

    void clear_dependencies() { m_dependencies.clear(); }

    void clear_params() { m_params.reset(); }

    virtual void realize() = 0;

    virtual void reload()
    {
        // Free old value
        reset();

        // Realize new value with saved arguments
        // This will also setup dependencies again
        realize();

        set_dirty(true);
    }

    virtual void reset() = 0;

    const std::any& params() const { return m_params; }
    std::any& params() { return m_params; }

protected:
    /// Optional parameters for lazy initialization
    std::any m_params;

    /// Dirty flag used to detect changes
    bool m_dirty = false;

    /// List of resources that depend on this resource data
    std::vector<std::shared_ptr<BaseResourceData>> m_dependencies;
};

} // namespace ui
} // namespace lagrange
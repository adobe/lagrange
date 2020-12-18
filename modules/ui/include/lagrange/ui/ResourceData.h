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

#include <lagrange/ui/BaseResourceData.h>
#include <lagrange/ui/ResourceFactory.h>

namespace lagrange {
namespace ui {

template <typename T>
class ResourceData : public BaseResourceData
{
public:
    ResourceData()
        : m_data(nullptr)
    {}

    ResourceData(std::unique_ptr<T> data)
        : m_data(std::move(data))
    {}

    ResourceData(std::shared_ptr<T> data)
        : m_data(std::move(data))
    {}

    /// Creates empty data and saves arguments for deferred realization
    template <typename... Args>
    ResourceData(Args... args)
    {
        m_params = std::tuple<Args...>(std::forward<Args>(args)...);
    }

    /// Updates resource data and shares ownership
    void set(std::shared_ptr<T> data) { m_data = std::move(data); }

    /// Returns raw pointer to data, may be null if not initialized
    T* get() { return m_data.get(); }

    /// Returns const raw pointer to data, may be null if not initialized
    const T* get() const { return m_data.get(); }

    void realize() override { ResourceFactory::realize<T>(*this); }

    void reset() override
    {
        set(std::shared_ptr<T>(nullptr));
        clear_dependencies();
    }

    const std::shared_ptr<T> & data() { return m_data; }

private:
    /// Shared ptr to data
    /// Using shared over unique, so that users can bring over resources from outside
    /// and retain co-ownership
    std::shared_ptr<T> m_data;
};
} // namespace ui

} // namespace lagrange